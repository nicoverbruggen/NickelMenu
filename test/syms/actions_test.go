package main

import (
	"fmt"
	"os"
	"os/exec"
	"path/filepath"
	"runtime"
	"strings"
	"testing"
)

func actionPaths(t *testing.T) (string, string) {
	t.Helper()
	root, err := filepath.Abs("../..")
	if err != nil {
		t.Fatal(err)
	}
	hook := os.Getenv("NICKELHOOK")
	if hook == "" {
		hook = filepath.Join(root, "NickelHook")
	}
	for _, compiler := range []string{"cc", "c++"} {
		if _, err := exec.LookPath(compiler); err != nil {
			t.Skipf("%s required: %v", compiler, err)
		}
	}
	return filepath.Join(root, "src"), hook
}

func actionRun(t *testing.T, name string, args ...string) string {
	t.Helper()
	output, err := exec.Command(name, args...).CombinedOutput()
	if err != nil {
		t.Fatalf("%s %q: %v\n%s", name, args, err, output)
	}
	return string(output)
}

func actionWrite(t *testing.T, name, contents string) {
	t.Helper()
	if err := os.WriteFile(name, []byte(contents), 0600); err != nil {
		t.Fatal(err)
	}
}

func TestActionRegistry(t *testing.T) {
	src, hook := actionPaths(t)
	fixture, err := os.ReadFile("testdata/registry.c")
	if err != nil {
		t.Fatal(err)
	}
	for _, major := range []string{"default", "5", "6"} {
		t.Run(major, func(t *testing.T) {
			dir := t.TempDir()
			flags := []string{"-Wall", "-Wextra", "-Werror", "-I" + src, "-I" + hook}
			if major != "default" {
				flags = append(flags, "-DNM_QT_MAJOR="+major)
			}
			var objects []string
			for _, name := range []string{"action", "util"} {
				obj := filepath.Join(dir, name+".o")
				args := append([]string{"-std=gnu11"}, flags...)
				args = append(args, "-c", filepath.Join(src, name+".c"), "-o", obj)
				actionRun(t, "cc", args...)
				objects = append(objects, obj)
			}
			for _, compiler := range []struct{ name, suffix, standard string }{{"cc", ".c", "gnu11"}, {"c++", ".cc", "gnu++11"}} {
				t.Run(compiler.name, func(t *testing.T) {
					source := filepath.Join(dir, "registry"+compiler.suffix)
					binary := filepath.Join(dir, "registry")
					actionWrite(t, source, string(fixture))
					args := append([]string{"-std=" + compiler.standard}, flags...)
					args = append(args, source)
					args = append(args, objects...)
					args = append(args, "-o", binary)
					actionRun(t, compiler.name, args...)
					actionRun(t, binary)
				})
			}
		})
	}
}

func TestUnsupportedQtMajor(t *testing.T) {
	src, _ := actionPaths(t)
	cmd := exec.Command("cc", "-x", "c", "-fsyntax-only", "-DNM_QT_MAJOR=7", "-I"+src, "-")
	cmd.Stdin = strings.NewReader("#include \"action.h\"\n")
	output, err := cmd.CombinedOutput()
	if err == nil || !strings.Contains(string(output), "Unsupported NM_QT_MAJOR") {
		t.Fatalf("expected Qt major rejection, got %v: %s", err, output)
	}
}

func TestActionConfig(t *testing.T) {
	src, hook := actionPaths(t)
	for _, major := range []string{"5", "6"} {
		t.Run(major, func(t *testing.T) {
			dir := t.TempDir()
			config := filepath.Join(dir, "config")
			if err := os.Mkdir(config, 0700); err != nil {
				t.Fatal(err)
			}
			binary := filepath.Join(dir, "parser")
			args := []string{"-std=gnu11", "-Wall", "-Wextra", "-Werror", "-I" + src, "-I" + hook, "-DNM_QT_MAJOR=" + major,
				"-DNM_CONFIG_DIR=\"" + config + "\"", "-DNM_CONFIG_DIR_DISP=\"test\""}
			if runtime.GOOS == "darwin" {
				args = append(args, "-Dst_mtim=st_mtimespec")
			}
			args = append(args, "testdata/config.c")
			for _, name := range []string{"action.c", "config.c", "util.c"} {
				args = append(args, filepath.Join(src, name))
			}
			args = append(args, "-o", binary)
			actionRun(t, "cc", args...)
			parse := func(text, failure string) string {
				t.Helper()
				actionWrite(t, filepath.Join(config, "menu"), text)
				out, err := exec.Command(binary).CombinedOutput()
				if failure == "" {
					if err != nil {
						t.Fatalf("parse: %v: %s", err, out)
					}
				} else {
					exit, ok := err.(*exec.ExitError)
					if !ok || exit.ExitCode() != 2 || !strings.Contains(string(out), failure) {
						t.Fatalf("expected %q, got %v: %s", failure, err, out)
					}
				}
				return string(out)
			}
			equal := func(got, want string) {
				t.Helper()
				if got != want {
					t.Fatalf("got %q, want %q", got, want)
				}
			}
			var configText, expected strings.Builder
			expected.WriteString("6\n")
			for i, loc := range []string{"main", "reader", "browser", "library", "selection", "selection_search"} {
				fmt.Fprintf(&configText, "menu_item:%s:%s:dbg_msg:ok\n", loc, loc)
				fmt.Fprintf(&expected, "%d|%s|11\n", i+1, loc)
			}
			equal(parse(configText.String(), ""), expected.String())
			for _, action := range []string{"nickel_extras", "nickel_bluetooth"} {
				equal(parse("menu_item:main:Kept:"+action+":test\nchain_failure:dbg_msg:failed\nchain_success:dbg_msg:passed\nchain_always:skip:-1\n", ""), "1\n1|Kept|11|01|10|11\n")
			}
			for _, c := range []struct{ text, failure string }{
				{"menu_item:main:X:unknown:arg\n", "unknown action"},
				{"menu_item:wrong:X:dbg_msg:arg\n", "unknown location"},
				{"chain_failure:dbg_msg:arg\n", "unexpected chain"},
				{"menu_item:main:X:dbg_msg\n", "expected argument"},
			} {
				parse(c.text, c.failure)
			}
			configText.Reset()
			for i := 0; i < 50; i++ {
				fmt.Fprintf(&configText, "menu_item:main:X%d:dbg_msg:ok\n", i)
			}
			if out := parse(configText.String(), ""); !strings.HasPrefix(out, "50\n") {
				t.Fatalf("wrong item count: %s", out)
			}
			parse(configText.String()+"menu_item:main:X50:dbg_msg:ok\n", "too many menu items")
			for _, name := range []string{".hidden", "backup~", "#backup#", "backup.swp", "desktop.ini"} {
				actionWrite(t, filepath.Join(config, name), "invalid")
			}
			equal(parse("menu_item:main:Recovered:dbg_msg:ok\n", ""), "1\n1|Recovered|11\n")
		})
	}
}
