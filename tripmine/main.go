package main

/*
	-----[ CLI Tool Handler ]-----
	This GO file handles the cli
	usage.

	Note:
		This runtime only works when it is compiled.
*/

import (
	"os"
	"tripmine/core"
)

func imager(args... string) {}

func loader(args... string) {}

func manager(args... string) {}

func server(args... string) {}

func main() {
	switch os.Args[1] {
	case "run":
		core.Run_run(os.Args[2:])
	}
}
