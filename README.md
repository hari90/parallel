# parallel
Run commands in parallel

## Usage
./parallel [-n <parallelism count>] '<command1>' '<command2>' ...
    Each command is broken down by spaces and double quoted(") strings are treated as a single argument.
    To escape a quote, use teo double quotes("")..
    To escape a single quote, use ''\''.

## Example
./parallel 'echo "1"' 'echo "Quoted ""2"""' -n 3 'sleep 1' 'ysqlsh  -c "SELECT version(),'\''hari'\''"'
