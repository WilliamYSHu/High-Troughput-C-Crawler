General Introduction:
This is a multi-thread mart crawler written in C++.
Each thread keeps its own cookie to mimic real users.

----------------------------------------------------------------------------------------
How to compile: make mcrawl
----------------------------------------------------------------------------------------
Usage:
mcrawl [ -n max-flows ] [ -h hostname ] [ -p port ] [-f localdirectory]
[ -n ] : number of maximum threads your crawler can
spawn to perform a cooperative crawl
[ -h ] : hostname of the server to crawl data from
[ -p ] : port number on the server where the web server is
running
[ -f ]: local directory to host the download files
More details on the command line:

-n option and -p option are optional. Their default value is 1 and 80.
options can be in any order.
--------------------------------------------------------------------------------------------
Warning:
1. Multithread/ bad_alloc
	Thread number less or equal to 4 works more stable.
	Since I used some STL and buffer, the program is not stable if there are too many threads.
	Sometimes it will cause bad_alloc error for requiring too much memory.
	Please simply rerun the program if there is bad_alloc error.

2. How files are saved
	Under the directory, files are saved flat.
	Each file name also contains the information of its directory in server.
	(dash "/" will be replaced by colon ":")

	For example, /index.html will be saved as :index.html

3. Dealing with 402 rate limit
	If there is a 402 error, the thread will get a new cookie.
	Also, the thread will simply put the task back to queue and crawl latter.

4. Dealing with 404 not Found
	The not found message will appear on stdout, but no file will be downloaded or saved.

5. File directory
	Please enter the full file directory (no ~)
	For example: /home/hys98/crawler

6. chunk transfer
	This crawler only work for server that use chunk transfer.

-------------------------------------------------------------------------------------------------
Error codes:
0 Operation successfully completed (will show on stdout if succeed)
1 input error
2 socket error
3 network error  (caused by fail in socket connect)
4 invalid hostname
5 server down (500)
6 chunk read error
7 Generic error

-------------------------------------------------------------------------------------------------
Design overview:

Work queue protected by mutex.
A set records crawled links.

Each thread:
Open a socket. Connect to server. Get a cookie.
while(still have work in queue):
	Get a task from queue
	download the file in queue
	parse if it is a html file, save the links to queue if they are not crawled before.

	If there is a 402:
		put work back in queue.
		Get a new cookie and continue.
