package common

import (
	"os/exec"
	"runtime"
	"strings"
)

func CheckProgressRunning(serverName string) (bool, error) {
	a := `ps ux | awk '/` + serverName + `/ && !/awk/ {print $2}'`
	pid, err := RunCommand(a)
	if err != nil {
		return false, err
	}
	return pid != "", nil
}

func runInWindows(cmd string) (string, error) {
	result, err := exec.Command("cmd", "/c", cmd).Output()
	if err != nil {
		return "", err
	}
	return strings.TrimSpace(string(result)), err
}

func RunCommand(cmd string) (string, error) {
	if runtime.GOOS == "windows" {
		return runInWindows(cmd)
	} else {
		return runInLinux(cmd)
	}
}

func runInLinux(cmd string) (string, error) {
	result, err := exec.Command("/bin/sh", "-c", cmd).Output()
	if err != nil {
		return "", err
	}
	return strings.TrimSpace(string(result)), err
}
