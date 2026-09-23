package main

import (
	"crypto/sha256"
	"debug/elf"
	"fmt"
	"os"
	"path/filepath"
	"regexp"
	"strconv"
	"strings"
)

type abiFunction struct {
	size   uint64
	digest string
}

func readABI(path string) (map[string][]abiFunction, error) {
	data, err := os.ReadFile(path)
	if err != nil {
		return nil, err
	}
	entries := map[string][]abiFunction{}
	pattern := regexp.MustCompile(`\{"([^"]+)", ([0-9]+), "([0-9a-f]{64})"\}`)
	for _, m := range pattern.FindAllStringSubmatch(string(data), -1) {
		size, err := strconv.ParseUint(m[2], 10, 64)
		if err != nil || size == 0 {
			return nil, fmt.Errorf("invalid ABI size for %s", m[1])
		}
		entries[m[1]] = append(entries[m[1]], abiFunction{size, m[3]})
	}
	if len(entries) == 0 {
		return nil, fmt.Errorf("no ABI checks in %s", path)
	}
	return entries, nil
}

func matchesABI(code []byte, alternatives []abiFunction) bool {
	digest := fmt.Sprintf("%x", sha256.Sum256(code))
	for _, candidate := range alternatives {
		if uint64(len(code)) == candidate.size && digest == candidate.digest {
			return true
		}
	}
	return false
}

// An exported name can survive an incompatible private implementation change.
func checkABI(source, directory string) error {
	catalog := "compat_data.h"
	entries, err := readABI(filepath.Join(source, catalog))
	if err != nil {
		return err
	}
	library, err := elf.Open(filepath.Join(directory, "libnickel.so.1.0.0"))
	if err != nil {
		return err
	}
	defer library.Close()
	if library.Class != elf.ELFCLASS32 || library.Machine != elf.EM_ARM {
		return fmt.Errorf("ABI checks require the ARM32 firmware library")
	}
	symbols, err := library.DynamicSymbols()
	if err != nil {
		return err
	}
	defined := map[string]elf.Symbol{}
	for _, symbol := range symbols {
		if symbol.Section != elf.SHN_UNDEF {
			defined[symbol.Name] = symbol
		}
	}
	failed := false
	// Allocation dependencies are expressed as class names, so the ordinary
	// mangled-symbol annotations cannot find them.
	for _, filename := range []string{"nickelmenu.cc", "action_cc.cc"} {
		data, err := os.ReadFile(filepath.Join(source, filename))
		if err != nil {
			return err
		}
		calls := regexp.MustCompile(`(?:nh|nm)_native_(?:storage|type|size)\(([^)]*)\)`)
		names := regexp.MustCompile(`"([A-Za-z][A-Za-z0-9]*)"`)
		for _, call := range calls.FindAllStringSubmatch(string(data), -1) {
			for _, match := range names.FindAllStringSubmatch(call[1], -1) {
				name := match[1]
				symbol := fmt.Sprintf("_ZN9QtPrivate25QMetaTypeInterfaceWrapperI%d%sE8metaTypeE", len(name), name)
				_, ok := defined[symbol]
				if !ok {
					fmt.Printf("[ERR] allocation metadata missing: %s\n", name)
					failed = true
				}
			}
		}
		for _, match := range regexp.MustCompile(`nm_resolve\("(_ZTV[^"]+)"\)`).FindAllStringSubmatch(string(data), -1) {
			if symbol, ok := defined[match[1]]; !ok || symbol.Size != 28 {
				fmt.Printf("[ERR] Settings vtable missing or changed: %s\n", strings.TrimPrefix(match[1], "_ZTV"))
				failed = true
			}
		}
	}
	for name, alternatives := range entries {
		symbol, ok := defined[name]
		var code []byte
		if ok {
			address := symbol.Value &^ 1 // ARM Thumb function pointers carry a low bit.
			for _, segment := range library.Progs {
				if segment.Type != elf.PT_LOAD || segment.Flags&(elf.PF_R|elf.PF_X) != elf.PF_R|elf.PF_X {
					continue
				}
				if address < segment.Vaddr || address-segment.Vaddr >= segment.Filesz {
					continue
				}
				offset := address - segment.Vaddr
				if symbol.Size == 0 || symbol.Size > segment.Filesz-offset {
					continue
				}
				code = make([]byte, symbol.Size)
				if _, err := segment.ReadAt(code, int64(offset)); err != nil {
					return err
				}
				break
			}
		}
		if len(code) == 0 {
			fmt.Printf("[ERR] private function missing, inaccessible or incompatible: %s\n", name)
			failed = true
		} else if !matchesABI(code, alternatives) {
			fmt.Printf("[INF] private function differs from reference; runtime testing needed: %s\n", name)
		}
	}
	if failed {
		return fmt.Errorf("private ABI checks failed")
	}
	fmt.Printf("[INF] %d private functions checked\n", len(entries))
	return nil
}
