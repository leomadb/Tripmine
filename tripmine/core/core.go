package core

/*
	----[ Core Handler ]----
	This handles a cube's
	input/output and syscalls
*/

import (
	"fmt"
	"log"
	"os"
	"os/exec"
	"syscall"
	"strconv"
)

func must(err error) {
	if err != nil {
		fmt.Fprintln(os.Stderr, "error:", err)
	}
}

func Run_run(args []string) {
	// For now !!
	if len(args) > 1 {
		e := fmt.Errorf("too many arguments")
		log.Fatal(e)
	}

	// Start attach
	cmd := exec.Command("bash")
	cmd.SysProcAttr = &syscall.SysProcAttr{
		Ptrace: true,
	}
	cmd.Env = []string{
		"PS1='$ '",
	}
	must(cmd.Start())
	pid := cmd.Process.Pid

	// Loop :)
	for {
		// Check for exits
		var ws syscall.WaitStatus
		_, err := syscall.Wait4(pid, &ws, 0, nil)
		must(err)

		// Get registry
		var regs syscall.PtraceRegs
		syscall.PtraceGetRegs(pid, &regs)

		if ws.Exited() {
			fmt.Printf("Cube exitted with: %s\n", strconv.Itoa(ws.ExitStatus()))
			break
		}

		if ws.Stopped() {
			fmt.Println("Syscall: ", regs.Orig_rax)
		}
	}
}
