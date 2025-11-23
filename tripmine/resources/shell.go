package main

import (
	"bufio"
	"fmt"
	"io"
	"os"
	"os/exec"
	"os/signal"
	"path/filepath"
	"strings"
	"syscall"
)

// --- 1. TOKENIZER (Handles quotes and spaces) ---
func tokenize(input string) ([]string, error) {
	var tokens []string
	var currentToken strings.Builder
	inQuote := false
	quoteChar := rune(0)

	for _, r := range input {
		switch {
		case inQuote:
			if r == quoteChar {
				inQuote = false // End quote
			} else {
				currentToken.WriteRune(r)
			}
		case r == '"' || r == '\'':
			inQuote = true
			quoteChar = r
		case r == ' ' || r == '\t':
			if currentToken.Len() > 0 {
				tokens = append(tokens, currentToken.String())
				currentToken.Reset()
			}
		case r == '|' || r == '>' || r == '<':
			// Handle operators as separate tokens if they are adjacent to text
			if currentToken.Len() > 0 {
				tokens = append(tokens, currentToken.String())
				currentToken.Reset()
			}
			tokens = append(tokens, string(r))
		default:
			currentToken.WriteRune(r)
		}
	}
	if inQuote {
		return nil, fmt.Errorf("unclosed quote")
	}
	if currentToken.Len() > 0 {
		tokens = append(tokens, currentToken.String())
	}
	return tokens, nil
}

// --- 2. COMMAND STRUCTURE ---
type Command struct {
	Name       string
	Args       []string
	StdinFile  string
	StdoutFile string
	AppendOut  bool
}

func parseCommands(tokens []string) ([]*Command, error) {
	var cmds []*Command
	current := &Command{}

	for i := 0; i < len(tokens); i++ {
		token := tokens[i]

		switch token {
		case "|":
			if current.Name == "" {
				return nil, fmt.Errorf("empty command before pipe")
			}
			cmds = append(cmds, current)
			current = &Command{}
		case ">", ">>":
			if i+1 >= len(tokens) {
				return nil, fmt.Errorf("missing filename after %s", token)
			}
			current.StdoutFile = tokens[i+1]
			current.AppendOut = (token == ">>")
			i++ // Skip filename
		case "<":
			if i+1 >= len(tokens) {
				return nil, fmt.Errorf("missing filename after <")
			}
			current.StdinFile = tokens[i+1]
			i++
		default:
			if current.Name == "" {
				current.Name = token
			} else {
				current.Args = append(current.Args, token)
			}
		}
	}
	if current.Name != "" {
		cmds = append(cmds, current)
	}
	return cmds, nil
}

// --- 3. BUILT-INS (For Distroless Survival) ---
func runBuiltin(cmd *Command, stdin io.Reader, stdout io.Writer, stderr io.Writer) (bool, error) {
	switch cmd.Name {
	case "cd":
		dir := "/"
		if len(cmd.Args) > 0 {
			dir = cmd.Args[0]
		}
		return true, os.Chdir(dir)
	
	case "pwd":
		cwd, _ := os.Getwd()
		fmt.Fprintln(stdout, cwd)
		return true, nil
	
	case "exit":
		os.Exit(0)
		return true, nil

	case "export":
		if len(cmd.Args) > 0 {
			parts := strings.SplitN(cmd.Args[0], "=", 2)
			if len(parts) == 2 {
				os.Setenv(parts[0], parts[1])
			}
		}
		return true, nil

	case "env":
		for _, e := range os.Environ() {
			fmt.Fprintln(stdout, e)
		}
		return true, nil

	case "echo":
		fmt.Fprintln(stdout, strings.Join(cmd.Args, " "))
		return true, nil

	// --- EMERGENCY FILESYSTEM TOOLS ---
	case "ls":
		dir := "."
		if len(cmd.Args) > 0 {
			dir = cmd.Args[0]
		}
		entries, err := os.ReadDir(dir)
		if err != nil {
			return true, err
		}
		for _, e := range entries {
			suffix := ""
			if e.IsDir() { suffix = "/" }
			fmt.Fprintf(stdout, "%s%s  ", e.Name(), suffix)
		}
		fmt.Fprintln(stdout, "")
		return true, nil

	case "cat":
		if len(cmd.Args) == 0 {
			io.Copy(stdout, stdin) // Echo stdin
			return true, nil
		}
		for _, fname := range cmd.Args {
			f, err := os.Open(fname)
			if err != nil {
				fmt.Fprintf(stderr, "cat: %v\n", err)
				continue
			}
			io.Copy(stdout, f)
			f.Close()
		}
		return true, nil
		
	case "mkdir":
		if len(cmd.Args) == 0 { return true, fmt.Errorf("mkdir: missing operand") }
		return true, os.MkdirAll(cmd.Args[0], 0755)
		
	case "touch":
		if len(cmd.Args) == 0 { return true, fmt.Errorf("touch: missing operand") }
		f, err := os.OpenFile(cmd.Args[0], os.O_RDONLY|os.O_CREATE, 0644)
		if err == nil { f.Close() }
		return true, err
	}

	return false, nil // Not a builtin
}

// --- 4. MAIN LOOP ---
func main() {
	reader := bufio.NewReader(os.Stdin)
	fmt.Println("Tripmine Shell v3.0 (Full Logic)")

	// Ignore Ctrl+C in the shell itself (pass to children)
	c := make(chan os.Signal, 1)
	signal.Notify(c, os.Interrupt, syscall.SIGTERM)
	go func() {
		for range c {
			// Do nothing, just prevent shell exit.
			// In a real shell, this clears the current line.
			fmt.Print("\n^C") 
		}
	}()

	basePath := os.Getenv("TRIP_PATH")
	if basePath == "" { basePath = "/bin" } // Default fallback

	for {
		cwd, _ := os.Getwd()
		fmt.Printf("\033[1;32mtrip\033[0m:\033[1;34m%s\033[0m$ ", filepath.Base(cwd))

		inputLine, err := reader.ReadString('\n')
		if err != nil { break } // EOF
		inputLine = strings.TrimSpace(inputLine)
		if inputLine == "" { continue }

		// 1. Tokenize
		tokens, err := tokenize(inputLine)
		if err != nil {
			fmt.Printf("Syntax error: %v\n", err)
			continue
		}

		// 2. Parse into Pipelines
		commands, err := parseCommands(tokens)
		if err != nil {
			fmt.Printf("Parse error: %v\n", err)
			continue
		}

		// 3. Execute Pipeline
		var lastStdin io.Reader = nil // Start with nil (default to os.Stdin later)
		
		for i, cmdStruct := range commands {
			// Setup Pipe
			var pipeReader *os.File
			var pipeWriter *os.File
			
			isLast := (i == len(commands)-1)

			// Determine Input
			var cmdStdin io.Reader
			if cmdStruct.StdinFile != "" {
				f, err := os.Open(cmdStruct.StdinFile)
				if err != nil {
					fmt.Printf("Error opening input: %v\n", err)
					break
				}
				defer f.Close()
				cmdStdin = f
			} else if lastStdin != nil {
				cmdStdin = lastStdin
			} else {
				cmdStdin = os.Stdin
			}

			// Determine Output
			var cmdStdout io.Writer
			if cmdStruct.StdoutFile != "" {
				flags := os.O_WRONLY | os.O_CREATE | os.O_TRUNC
				if cmdStruct.AppendOut { flags = os.O_WRONLY | os.O_CREATE | os.O_APPEND }
				f, err := os.OpenFile(cmdStruct.StdoutFile, flags, 0644)
				if err != nil {
					fmt.Printf("Error opening output: %v\n", err)
					break
				}
				defer f.Close()
				cmdStdout = f
			} else if !isLast {
				pipeReader, pipeWriter, _ = os.Pipe()
				cmdStdout = pipeWriter
			} else {
				cmdStdout = os.Stdout
			}

			// Check Built-in
			isBuiltin, err := runBuiltin(cmdStruct, cmdStdin, cmdStdout, os.Stderr)
			if isBuiltin {
				if err != nil { fmt.Printf("%s: %v\n", cmdStruct.Name, err) }
				if pipeWriter != nil { pipeWriter.Close() }
				lastStdin = pipeReader
				continue
			}

			// External Command
			execPath := cmdStruct.Name
			if !strings.Contains(execPath, "/") {
				execPath = filepath.Join(basePath, cmdStruct.Name)
				// Quick check if it exists, otherwise try PATH
				if _, err := os.Stat(execPath); os.IsNotExist(err) {
					// Try lookup in PATH
					if lp, err := exec.LookPath(cmdStruct.Name); err == nil {
						execPath = lp
					}
				}
			}

			sysCmd := exec.Command(execPath, cmdStruct.Args...)
			sysCmd.Stdin = cmdStdin
			sysCmd.Stdout = cmdStdout
			sysCmd.Stderr = os.Stderr

			if err := sysCmd.Start(); err != nil {
				fmt.Printf("Error starting %s: %v\n", cmdStruct.Name, err)
				if pipeWriter != nil { pipeWriter.Close() }
				break // Stop pipeline
			}

			// Close write-end of pipe so next process gets EOF
			if pipeWriter != nil { pipeWriter.Close() }
			
			// If it's the last command, wait for it
			if isLast {
				sysCmd.Wait()
			}
			
			lastStdin = pipeReader
		}
	}
}