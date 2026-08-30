# Digital Electronics and VLSI Internship Projects

This repo has the two VHDL projects I made this as part of my Digital Electronics and VLSI internship with Codec Technologies.

I didn't have access to an FPGA board or any paid tools, I did everything using online tools and simulation using GHDL (installed locally on my laptop) and verified the outputs from the terminal.

## Projects

### 1. Smart Digital Lock System
A digital lock made in VHDL that checks a 4-bit password. If someone enters the wrong code 3 times, it triggers an alert signal. There's also a master reset to clear everything.

Folder: `smart-digital-lock/`

### 2. FPGA Traffic Light Controller with Priority System
A traffic light controller for two roads (A and B) with the usual red-yellow-green sequence, plus an emergency override — if the emergency signal goes high, the controller gives priority to green to the main road until the emergency clears.

Folder: `traffic-light-controller/`

## Tools used

- VHDL (std_logic_1164, std_logic_unsigned)
- GHDL 6.0.0 (mcode backend) for compiling and simulation (got from github repo)
- Ran everything through Command Prompt on Windows 10

## How I ran it

```
ghdl -a --std=08 -fsynopsys <design_file>.vhd
ghdl -a --std=08 -fsynopsys <testbench_file>.vhd
ghdl -e --std=08 -fsynopsys <testbench_entity>
ghdl -r --std=08 -fsynopsys <testbench_entity>
```

Screenshots of the simulation runs are inside each project folder.

## Note

This was a online/simulation-based internship  no hardware was used. All designs were tested using GHDL only.
