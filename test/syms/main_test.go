package main

import (
	"os"
	"path/filepath"
	"testing"
)

func TestVersionRanges(t *testing.T) {
	for _, c := range []struct {
		version, start, end string
		want                bool
	}{
		{"4.46.23836", "4.6", "4", true},
		{"5.18.270971", "4.6", "4", false},
		{"4.30.18838", "4.20.14622", "4.30.18838", true},
		{"4.31.19086", "4.20.14622", "4.30.18838", false},
		{"5.18.270971", "5.18.270971", "*", true},
		{"4.46.23836", "5.18.270971", "*", false},
		{"4.5.0", "4.6", "4", false},
	} {
		if got := versionInRange(c.version, c.start, c.end); got != c.want {
			t.Errorf("%s in %s..%s = %v, want %v", c.version, c.start, c.end, got, c.want)
		}
	}
}

func TestLibraryAnnotations(t *testing.T) {
	dir := t.TempDir()
	if err := os.WriteFile(filepath.Join(dir, "action.cc"), []byte("//libnickel 4.6 4 old alternative\n//libqt6gui 5.18.270971 * orientation\n"), 0600); err != nil {
		t.Fatal(err)
	}
	checks, err := FindSymChecks(dir)
	if err != nil {
		t.Fatal(err)
	}
	if len(checks) != 2 {
		t.Fatalf("got %d checks", len(checks))
	}
	if checks[0].Library != "libnickel.so.1.0.0" || len(checks[0].Symbols) != 2 || checks[1].Library != "libQt6Gui.so.6" {
		t.Fatalf("wrong libraries or alternatives: %+v", checks)
	}
}

func TestMissingSourceIsAnError(t *testing.T) {
	if _, err := FindSymChecks(filepath.Join(t.TempDir(), "missing")); err == nil {
		t.Fatal("missing source directory passed")
	}
}
