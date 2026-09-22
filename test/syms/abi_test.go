package main

import (
	"crypto/sha256"
	"fmt"
	"os"
	"path/filepath"
	"testing"
)

func TestABIRejectsChangedBytesAndWrongSize(t *testing.T) {
	code := []byte{1, 2, 3, 4}
	digest := fmt.Sprintf("%x", sha256.Sum256(code))
	alternatives := []abiFunction{{4, "unknown"}, {4, digest}}
	if !matchesABI(code, alternatives) {
		t.Fatal("valid alternative rejected")
	}
	if matchesABI([]byte{1, 2, 3, 5}, alternatives) {
		t.Fatal("changed code accepted")
	}
	if matchesABI(code, []abiFunction{{3, digest}}) {
		t.Fatal("wrong size accepted")
	}
	if matchesABI(nil, alternatives) {
		t.Fatal("missing symbol accepted")
	}
}

func TestABICatalogRequiresEntriesAndPreservesAlternatives(t *testing.T) {
	path := filepath.Join(t.TempDir(), "compat_data.h")
	if err := os.WriteFile(path, []byte("// no entries"), 0600); err != nil {
		t.Fatal(err)
	}
	if _, err := readABI(path); err == nil {
		t.Fatal("empty catalog accepted")
	}
	digest := fmt.Sprintf("%x", sha256.Sum256([]byte{1}))
	data := fmt.Sprintf("{\"symbol\", 1, \"%s\"},\n{\"symbol\", 2, \"%s\"},\n", digest, digest)
	if err := os.WriteFile(path, []byte(data), 0600); err != nil {
		t.Fatal(err)
	}
	entries, err := readABI(path)
	if err != nil || len(entries["symbol"]) != 2 {
		t.Fatalf("alternatives lost: %v %v", entries, err)
	}
}
