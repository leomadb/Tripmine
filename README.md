
# Tripmine (Runtime)

## How to use?

An alternative for serverless web hosting.

Bundles your code into an image and you can deploy the box and make it run quick.


![Logo](https://i.imgur.com/ATJMgCs.png)


## Concept
The concept of tripmine is to make cloud apps by instead of using a VM, it makes an isolated box which has no access anywhere and runs locally on your machine, little to no cold-startup.

## Get Started

Clone into Tripmine-v1.0

```bash
  git clone https://github.com/leomadb/Tripmine
```

Go to the project directory

```bash
  cd Tripmine
```

Start the install shell script, installs everything for you.

```bash
  ./install.sh
```


## Create your first app

To start and make your first Tripmine project, use this command.

```bash
  tripmine init my-app --template python
```

Available templates:
```
  --template
      python
      go
      node
      sql
      rust
      cs  (C#)
      cpp (C++)
```

You will now have this in your project directory:
```
  my-app/
    .tripignore   **IMPORTANT**
    .tconfig.yml  **IMPORTANT**
    .tripmine/
      bin/
        python3.waitfor **IMPORTANT**
    main.py
```

To bundle your app:
```bash
  tripmine build .
```

With your new file, `my-app.tbi`,
sign it and put you as the author
```bash
  tripmine make --trust-at https://tripmine.embemb.com/authors --sign leomadb
  tripmine sign my-app.tbi leomadb.tsign
```
Now, you are ready to use this image! (New file: `my-app.tpm`)

If you use CLI:
```bash
  tripmine run my-app.tpm
    LOG   Loading my-app into cache
    LOG   Unwrapping mirrors...
   SHELL  $ which apt
   SHELL  $ wget https://www.python.org/ftp/python/3.14.1/Python-3.14.1.tgz
    TAR   Unwrapping tarball to /my-app/tmp/bin/Python-3.14.1
   SHELL  $ ./configure --prefix=$TRIPMINE_IMAGES_LOADED/my-app/system/bin
   SHELL  $ make
   SHELL  $ make install
    LOG   Successfully mirrored python3
   ENTRY  $ python3 main.py
```
## Config Documentation `YAML`

```yaml
# .tconfig.yaml

# Defaults for services and tasks
defaults: &defaults
  image: image:1.2.3
  env: {}                 # enviroment variables go here
  # Replication of cubes if it fills
  replication:
    enabled: true
    max_replicates: 8
    replicate_when: "max_cpu || max_mem || max_disk"

# Networking configuration
net:
  ports: 
    - "8000:8000"    # Exposes port 8000 from lo to eth0, use *:port to NOT expose a port
  routes:
    # Map of port -> purpose/notes
    8000: "take&give&request"     # Opens port 8000 for the uses laid out

# Disk configuration and constraints
disk:
  needs:
    - "${HOME}/Documents/*"    # glob expansion handled by runtime (documented behavior)
  mode: flexible
  limits:
    max_size: 1G               # explicit size unit

# Entry point / command
entry:
  command: ["python3", "main.py"]

# Performance constraints
performance:
  cpu_cores: 1                 # explicit cores (integer)
  memory: 512M                 # memory with unit
  disk_io: "flexible"          # clarify as IO mode if applicable

# Rollback/snapshots (if the program fails, rollback to retry)
rollback:
  enabled: true
  last_snapshot: "custom"      # can be: "latest" | "custom"
  trail_snapshots: true        # whether to store every snapshot
  max_trail: null              # unlimited trail
  max_retries: 3
  custom_rollback:
    # Ordered steps for custom rollback; keys as integers preserve sequence
    1:
      last_snapshot: 10
    2:
      last_snapshot: 20
    3:
      last_snapshot: 0

# Mirrors and resources catalog (NECESSARY for runtime!)
mirrors:
  # Runtime mirror for Python 3
  python3: &python3_mirror
    name: "python3"
    type: "runtime"
    source: "tripmine://python/3.11.9/linux-arm64"
    # offline-ok allows to use the computer's file.
    # general defines the usage of the image loaded mirrors.
    # online-only defines if only to use the latest downloaded version of the source.
    policy: "offline-ok=true, general=true, online-only=false"
  # Generic commands mirror
  flexible: &generic_mirror
    name: "*"
    type: "generic"
    source: "tripmine://generic_commands/linux"

```
## What happens inside a Tripmine Cube?

A few very important and cool things happen in the process of cube running/making.

#### 1.)  Unpacking
Tripmine Desktop or Tripmine CLI automatically unpacks the image into a folder called `/Mines/` inside, all the magic happens.

Inside the `/Mines/` folder lives the current working cube:
```
Mines/
    aXddE4n9U825F32n89Ff0opP_92aWrQQ/
        headers/
            signature.tsign
            trustat.txt
            .env
        frame/
            main.py
        mirrors/
            python3.mirror
            ls.mirror
            cd.mirror
            cat.mirror
            nano.mirror
        system-copy/ 
            [empty]
```
Before you freak out, the big alphanumeric number is just the TripID of the cube running withing the runtime, the `headers/` folder is just there to let the runtime know its trustworthy and where to confirm, and specially the enviroment values.

The `frame/` has the code that the image stores.

The `mirrors/` folder containts the .mirror files that when mentioned to the runtime, it can resolve the IO.

#### 2.)  Controlled Execution
The runtime then will execute the entry command in a secure enviroment, it only has the commands and files from the mirrors and nothing else. Also reports back what is happening to you.
```
Tripmine runtime <-IO/NET-> frame
```
This demonstration basically describes how tripmine controls what goes in and out based on what is allowed or needed.
