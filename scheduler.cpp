#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <math.h>
#include <ctype.h>
#include <string.h>

typedef struct process {
	int pid;
	int burst;
	int arrival;
	int priority;
	
	int response;
	int waiting;
	int turnaround;
	int firstrun;
	int lastrun;
} process;

typedef struct job {
	process *p;
	int length;
	int start;
	int priority;
} job;

typedef struct node {
	job *j;
	struct node *prev;
	struct node *next;
} node;

typedef int (*comp)(const void *, const void *);
int cmpproc(process *pa, process *pb) // chronological sorting, higher priority first + shortest burst first
{
	int c = pa->arrival - pb->arrival;
	if (!c) {
		c = pa->priority - pb->priority;
		if (!c)
			c = pa->burst - pb->burst;
	}
	return c ? c / abs(c) : 0;
}

int cmplast(process *pa, process *pb) // order by process end time
{
	int c = pa->lastrun - pb->lastrun;
	return c ? c / abs(c) : 0;
}

int min(int a, int b) { return a < b ? a : b; } // min function

double avg_response = 0, avg_waiting = 0, avg_turnaround = 0;
int response, waiting, turnaround, qt;
int numjobs = 0, ct = 0;

node *head = NULL, *tail = NULL; // process queue

void enqueue(job *j)
{
	node *n = (node *)malloc(sizeof(node));
	n->j = j;
	n->next = n->prev = NULL;
	if (head == NULL)
		head = tail = n; // empty queue, set head and tail
	else {
		tail->next = n; // append tail with the new node
		n->prev = tail;
		tail = n;
	}
}

bool isEmpty()
{
	return head == NULL;
}

job *dequeue()
{
	if (isEmpty())
		return NULL;
	node *curnode = head;
	job *j = curnode->j;
	head = head->next;
	free(curnode); // we don't care node object, but job object referenced
	if (isEmpty())
		tail = NULL;
	if (head)
		head->prev = NULL;
	return j;
}

job *toJob(process *p) // transform a process into a job
{
	job *j = (job *)malloc(sizeof(job));
	j->length = p->burst;
	j->p = p;
	return j;
}

job *createJobs(process list[], int t) // create list of jobs from process list
{
	int amount, base_arrival, njobs = 0;
	job *jobs = (job *)malloc(sizeof(job) * t);
	for (int i = 0; i < t; i++) {
		amount = list[i].burst;
		job *jb = toJob(&list[i]);
		jb->length = amount;
		jb->start = list[i].arrival;
		jobs[njobs++] = *jb;
	}
	return (job *)jobs;
}

void calculate(process list[], int t)
{
	job *jobs = createJobs(list, t);
	for (int i = 0; i < t; i++)
		enqueue(&jobs[i]);
	// calculating metrics for each process, here worry nothing about time slice
	while (!isEmpty()) {
		job *jb = dequeue();
		process *p = jb->p;
		if (ct < p->arrival)
			ct = p->arrival; // jump to the next work over time gap, if exists
		if (!p->firstrun) { // if run for first time
			p->firstrun = 1; // mark it
			p->response = ct - p->arrival; // calculate response time
		}
		if (p->lastrun == -1)
			p->waiting = ct - p->arrival; // the first wait
		else
			p->waiting += ct - p->lastrun; // wait more
		ct += jb->length;
		p->lastrun = ct;
		p->turnaround = ct - p->arrival;
	}
}

int main(int argc, char *argv[]) {
	if (argc != 3) {
		perror("Check your input");
		exit(EXIT_FAILURE);
	}
	FILE *f = fopen(argv[1], "r");
	if (!f) {
		perror("Error opening file");
		exit(EXIT_FAILURE);
	}
	int t, j, i = 0;
	fscanf(f, "%d", &t);
	process list[t];
	int minarrival = 999;
	while (i < t && fscanf(f, "%d %d %d %d", &list[i].pid, &list[i].burst, &list[i].arrival, &list[i].priority)) {
		list[i].waiting = list[i].turnaround = list[i].firstrun = 0;
		list[i].lastrun = -1;
		if (list[i].arrival < minarrival)
			minarrival = list[i].arrival;
		i++;
	}
	for (i = 0; i < t; i++)
		list[i].arrival -= minarrival; // set start time to zero for simplicity
	if (!strcmp(argv[2], "FCFS")) {
		// First-come first-served
		qsort(list, t, sizeof(process), (comp)cmpproc);
		for (i = 0; i < t; i++) {
			process *p = &list[i];
			p->response = p->waiting = ct - p->arrival;
			ct += p->burst;
			p->turnaround = ct - p->arrival;
			p->lastrun = ct;
			if (i + 1 < t && ct < list[i + 1].arrival) // jump to where work exist over time gap
				ct = list[i + 1].arrival;
		}
	} else if (!strcmp(argv[2], "SJF")) {
		// normal SJF
		qsort(list, t, sizeof(process), (comp)cmpproc);
		int min, k = 1;
		for (j = 0; j < t; j++) {
			ct += list[j].burst;
			min = list[k].burst;
			// find the shortest job coming within burst period of currently-running process
			for (i = j + 1; i < t; i++) {
				if (ct >= list[i].arrival && list[i].burst < min) {
					process temp = list[k];
					list[k] = list[i];
					list[i] = temp;
				}
			}
			k++;
		}
		ct = 0;
		calculate(list, t);
	} else if (!strcmp(argv[2], "SJFW")) {
		// preemptive SJF
		int end, smallest, count = 0;
		int sburst[t + 1];
		for (i = 0; i < t; i++)
			sburst[i] = list[i].burst;
		sburst[i] = 99;
		for (ct = 0; count != t; ct++) {
			smallest = t;
			for (i = 0; i < t; i++) {
				if (ct >= list[i].arrival && sburst[i] > 0 && sburst[i] < sburst[smallest])
					smallest = i; // find the shortest job
			}
			if (!list[smallest].firstrun) {
				list[smallest].response = ct - list[smallest].arrival;
				list[smallest].firstrun = 1;
			}
			if (--sburst[smallest] == 0) { // the shortest job is completed, calculate such metrics
				count++;
				end = ct + 1;
				list[smallest].waiting = end - list[smallest].arrival - list[smallest].burst;
				list[smallest].turnaround = end - list[smallest].arrival;
				list[smallest].lastrun = end;
			}
		}
	} else if (qt = atoi(argv[2])) {
		qsort(list, t, sizeof(process), (comp)cmpproc);
		int k = 1;
		enqueue(toJob(&list[0]));
		while (!isEmpty()) {
			job *j = dequeue();
			if (!j->p->firstrun) {
				j->p->response = ct - j->p->arrival;
				j->p->firstrun = 1;
			}
			if (j->p->lastrun != -1)
				j->p->waiting += ct - j->p->lastrun;
			else
				j->p->waiting = ct - j->p->arrival;
			ct += min(qt, j->length);
			while (k < t && list[k].arrival < ct) // enqueue any job within burst time of current job
				enqueue(toJob(&list[k++]));
			if (j->length > qt) { // split the job in case burst time > quantum, the latter one is enqueued
				job *jb = toJob(j->p);
				jb->length = j->length - qt;
				enqueue(jb);
			}
			j->p->lastrun = ct;
			j->p->turnaround = ct - j->p->arrival;
			if (isEmpty() && k < t) { // skip interval of zero job
				job *jb = toJob(&list[k++]);
				enqueue(jb); // and enqueue the first job found
				ct = jb->p->arrival;
			}
		}
	} else {
		perror("Unrecognized or incompatible arguments");
		exit(EXIT_FAILURE);
	}
	qsort(list, t, sizeof(process), (comp)cmplast);
	for (i = 0; i < t; i++) {
		printf("(pid = %d) %d %d %d\n", list[i].pid, response = list[i].response, waiting = list[i].waiting, turnaround = list[i].turnaround);
		avg_response += response;
		avg_waiting += waiting;
		avg_turnaround += turnaround;
	}
	avg_response /= t;
	avg_waiting /= t;
	avg_turnaround /= t;
	// per-process: response + waiting + turnaround
	// final: throughput + av.response + av.waiting + av.turnaround (%.2lf)
	printf("%.2lf %.2lf %.2lf %.2lf\n", (double)t / (ct + minarrival), avg_response, avg_waiting, avg_turnaround);
	fclose(f);
	return EXIT_SUCCESS;
}
