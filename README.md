# parallel
Run commands in parallel

## Usage
./parallel [-n <parallelism count>] <command1> <command2> ...

## Example
./parallel -n 2 "echo 1; sleep 1" "echo 2; sleep 2" "echo 3; sleep 3"
