# nodeParallel
A simple tool to run multiple command in parallel over multiple nodes. Each command be run on a single node. Very useful for short jobs, because it allows to shortcut the submission queue, and factorize the initialization.

## Installation
Simply execute `make -j`
Then add the folder to the PATH!

## Use the server
| Flag			| Description   					|
| ------------- |-----------------------------------|
| `-p n`		| Set the communication port to `n`	|
| `-d`			| Run as a daemon (in background)	|

## Use the client
| Flag			| Description   							|		|
| ------------- |-------------------------------------------|-------|
| `-p n`		| Set the communication port to `n`			|Both	|
| `-d`			| Run as a daemon (in background)			|Worker	|
| `-w`			| Set as a worker mode (execute the command)|Worker	|
| `-sac`		| Stop running after all command executed.	|Worker	|
| `-sa`			| Run as a daemon (in background)			|Both	|
| `-cmd`		| Command to execute						|Sub	|
| `-b`			| Run in blocking mode (wait that the command finish remotely <br>before returning, crtl+c is forwarded too)	|Sub	|

## How to use ?
To use nodeParallel, you need first to run the server `np_server`.
Then either
1.	Run some(1+) workers using on a single/multiple nodes. Example `np_client -sa serverAddress -w`, or multiple worker `parallel --lb -j4 "np_client -sa serverAddress -w" ::: {0..3}`
2.	Then simply run a command like `np_client -b -cmd sleep 2`, `np_client -b <<< sleep 2`or multiple such as `parallel -j4 -k "np_client -b -cmd echo " ::: {1..50}`

Either
1.	Run tasks such as `np_client -cmd sleep 2`, `np_client <<< sleep 2`or multiple such as `parallel -j4 -k "np_client -cmd echo " ::: {1..50}`
2.	The run some(1+) workers using on a single/multiple nodes. Example `np_client -sa serverAddress -sac -w`, or multiple worker `parallel --lb -j4 "np_client -sa serverAddress -sac -w" ::: {0..3}`


