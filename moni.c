#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/inotify.h>
#include <time.h>
#include <signal.h>

// ### STACK SMASH at file size ~ 220.000 ~ be careful
// either save log file or write bashfile to restart code
// thus /tmp/secret gets deleted

// error handler

#define handle(str) \
	do { perror(str);exit(EXIT_FAILURE); } while(0);


// size needed for 1 notify event

#define BUF_LEN ( (sizeof(struct inotify_event) + NAME_MAX + 1))

void log_operation(const char *text, struct inotify_event *i, int *t_fd, char **fullpath);
void display_event( struct inotify_event *, int * , char *[]);

void handler( int sig )
{
	unlink("/tmp/secret");
	exit(EXIT_FAILURE);
}

int main()
{
	system("rm -rf /tmp/secret");

	// change this file if system cant make so many watches, if to low 
	// increase it to 100000 , actually need 50000 but lets go sure :)

	FILE *check = popen("cat /proc/sys/fs/inotify/max_user_watches", "r");
	if ( !check ) handle("popen check");
	char check_buffer[256];
	fread(check_buffer, sizeof(char), sizeof(check_buffer), check);
	printf("check_buffer: %s\n", check_buffer);
	if ( atoi(check_buffer) < 100000 )
		system("echo '100000' > /proc/sys/fs/inotify/max_user_watches");

	system("echo '100000' > /proc/sys/fs/inotify/max_queued_events");

	// signal handler if programm is canceled by C-c

	struct sigaction action;
	memset(&action, 0, sizeof(action));
	action.sa_handler = handler;
	sigemptyset(&action.sa_mask);
	action.sa_flags = 0;
	if ( sigaction(SIGINT, &action, NULL) == -1 ) handle("sigaction");

	umask(0);

	// get all directorys on system including root

	FILE *fp = popen("sudo find /  -type d \\( -name 'sys' -o -name 'dev' -o -name 'proc' \\) -prune -o -type d -print" , "r");
	if ( fp == NULL )
		handle("popen");
	char *buffer = malloc( 5000000 * sizeof(char));
	fread(buffer, sizeof(char), 5000000, fp);



	int noty_fd, *wd;
	struct inotify_event *event;

	// inotify file descriptors

	char *p;
	wd = calloc(50000, sizeof(int));
	if ( !wd ) handle("calloc");

	char *full_path[50000];

	// init inotify

	noty_fd = inotify_init();
	if ( noty_fd == -1 )
		handle("inotify_init(): ");

	
	// LOG FILE

	int temp_fd = open("/tmp/secret", O_RDWR | O_CREAT, 0644);
	if ( temp_fd == -1 ) 
		handle("temp open");

	// get every path from this crazy read string above into single buffers
	p = strtok(buffer, "\n");
	full_path[0] = p;


	if ( strstr(p, "bin") )
		wd[0] = inotify_add_watch(noty_fd, p,  IN_ATTRIB | IN_DELETE | IN_CREATE);

	else
		wd[0] = inotify_add_watch(noty_fd, p,  IN_ATTRIB );

	if ( wd[0] == -1 )
		handle("add_watch [0]");



	// add watch for every directory

	int x = 1;
	while ((p = strtok(NULL, "\n")))
	{
		if ( strstr(p, "bin") )
			wd[x] = inotify_add_watch(noty_fd, p,  IN_ATTRIB | IN_DELETE | IN_CREATE);
		else
			wd[x] = inotify_add_watch(noty_fd, p,  IN_ATTRIB  );

		full_path[x] = p;
		if ( wd[x] == -1 )
			handle("add_watch");
		x++;
	}


	// need huge space for event space
	// increase if code crashes

	char *buf = malloc( 100000 * BUF_LEN );

	size_t n;
	char *ptr;


	for ( ;; )
	{
		n = read(noty_fd, buf, BUF_LEN); 	// read event

		for ( ptr = buf; ptr < buf + n; )
		{
			event = (struct inotify_event *)ptr;		  // cast memory to event
			display_event(event, &temp_fd, full_path); 	  // handle event
			ptr += sizeof(struct inotify_event) + event->len; // jump to next event
		}
	}

	// free everything

	free(wd);
	free(buf);
	free(buffer);

	unlink("/tmp/secret");

	exit(EXIT_SUCCESS);
}

void log_operation(const char *text, struct inotify_event *i, int *t_fd, char **fullpath ) {


  time_t cur;
  cur = time(NULL);
  char path_buffer[4096];
  strcat(path_buffer, fullpath[i->wd]);
  strcat(path_buffer, "/");
  strcat(path_buffer, i->name);
  dprintf(*t_fd,
          "####################\n%s\n\nfile %s: file "
          "created\n\n####################\n",
          ctime(&cur), path_buffer);
}

void display_event( struct inotify_event *i, int *t_fd, char **fullpath )
{
	if ( i->mask & IN_MODIFY )
		log_operation("file modified", i, t_fd, fullpath);
	if ( i->mask & IN_CLOSE_NOWRITE )
		log_operation("file readonly", i, t_fd, fullpath);
	if ( i->mask & IN_CLOSE_WRITE )
		log_operation("file written", i, t_fd, fullpath);
	if ( i->mask & IN_ATTRIB)
		log_operation("attribute modified", i, t_fd, fullpath);
	if ( i->mask & IN_OPEN )
		log_operation("file opened", i, t_fd, fullpath);
	if ( i->mask & IN_ACCESS )
		log_operation("file accessed", i, t_fd, fullpath);
	if ( i->mask & IN_DELETE)
		log_operation("file deleted", i, t_fd, fullpath);
	if ( i->mask & IN_CREATE)
		log_operation("file created", i, t_fd, fullpath);
}


