#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>


#define SET_SIGNALS_HANDLER(signals__, signals_size__, act__) \
	do { \
		for (unsigned int i__ = 0; i__ < signals_size__; ++i__) \
			if (sigaction(signals__[i__], &act__, NULL) < 0) \
				perror("Sigaction error"); \
	} while (0)


pid_t pid;
void (*old_stop_handler)(int);
void (*old_cont_handler)(int);

char child_sleep = 0;	// 1 if sleep, 0 otherwise


void sigchld_handler(int unused)	// Завершение родительского процесса, если ребёнок умер
{ exit(0); }


void sighup_handler(int unused)	// Игнорируем отключение родителя от терминала
{}


void terminate_handler(int unused)	// Завершение родителя и ребёнка
{
	kill(pid, SIGKILL);
	exit(5);
}


void stop_handler(int arg)
{
	kill(pid, SIGSTOP);
	old_stop_handler(arg);
}


void cont_handler(int arg)
{
	if (!child_sleep) kill(pid, SIGCONT);
	old_cont_handler(arg);
}


int main(int argc, char **argv)
{
	double percent = 0;
	
	if (argc < 3
		|| sscanf(argv[1], "%lf", &percent) != 1
		|| percent <= 0 || percent > 100) {
		fprintf(stderr, "Incorrect arguments!\n\tUsage: %s PERCENT COMMAND [...]\n", argv[0]);
		return 2;
	}
	
	pid = fork();
	if (pid == 0) {	// Код ребёнка
		pid = fork();
		if (pid == 0) {	// Код ребёнка
			if (execvp(argv[2], argv + 2)) {
				perror("Execvp error");
				return 1;
			}
		} else {	// Код родителя
			if (pid < 0) {
				perror("Fork error");
				return 1;
			}
			
			
			// Установка сигналов
			{	// Сигнал завершения ребёнка
				struct sigaction act = { .sa_handler = sigchld_handler,
										 .sa_mask = 0,
										 .sa_flags = SA_NOCLDSTOP };
				
				if (sigaction(SIGCHLD, &act, NULL) < 0) {
					perror("Sigaction error: can't set SIGCHLD signal handler");
					kill(pid, SIGKILL);
					return 1;
				}
			}
			
			
			{	// Сигнал отключения терминала
				struct sigaction act = { .sa_handler = sighup_handler,
										 .sa_mask = 0,
										 .sa_flags = 0 };
				
				if (sigaction(SIGHUP, &act, NULL) < 0)
					perror("Sigaction error: can't set SIGHUP signal handler");
			}
			
			
			{	// Завершающие сигналы
				const int terminate_signals[] = { SIGINT, SIGQUIT, SIGILL, SIGTRAP,
												  SIGABRT, SIGEMT, SIGFPE, SIGKILL,
												  SIGBUS, SIGSEGV, SIGSYS, SIGPIPE,
												  SIGALRM, SIGTERM, SIGXCPU, SIGXFSZ,
												  SIGVTALRM, SIGPROF, SIGUSR1, SIGUSR2 };
				
				struct sigaction act = { .sa_handler = terminate_handler,
										 .sa_mask = 0,
										 .sa_flags = 0 };
				SET_SIGNALS_HANDLER(terminate_signals, sizeof(terminate_signals), act);
			}
			
			
			{	// Останавливающие сигналы
				struct sigaction act = { .sa_handler = stop_handler,
										 .sa_mask = 0,
										 .sa_flags = 0 },
								 old_act;
				
				sigaction(SIGSTOP, &act, &old_act);
				old_stop_handler = old_act.sa_handler;
				
 				const int stop_signals[] = { SIGTSTP, SIGTTIN, SIGTTOU };
				SET_SIGNALS_HANDLER(stop_signals, sizeof(stop_signals), act);
			}
			
			
			{	// Продолжающий сигнал
				struct sigaction act = { .sa_handler = cont_handler,
										 .sa_mask = 0,
										 .sa_flags = 0 },
								 old_act;
				
				sigaction(SIGCONT, &act, &old_act);
				old_cont_handler = old_act.sa_handler;
			}
			
			
			{	// Собственно, рабочий цикл
				useconds_t sleep_cont = percent * 10000,
						   sleep_stop = 100000 - sleep_cont;
				
				while (1) {
					usleep(sleep_cont);	// Ребёнок работает
					kill(pid, SIGSTOP);	// ... усыпляем
					child_sleep = 1;
					
					usleep(sleep_stop);	// Ребёнок спит
					kill(pid, SIGCONT);	// ... будим
					child_sleep = 0;
				}
			}
		}
	} else {	// Код родителя
		if (pid < 0) {
			perror("Fork error");
			return 1;
		}
	}
	return 0;
}