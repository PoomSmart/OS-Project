#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <math.h>
#include <ctype.h>
#include <string.h>

#define MAXJOBS 300

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
	node *prev;
	node *next;
} node;

typedef int (*comp)(const void *, const void *);
int cmpproc(process *pa, process *pb)
{
	int c = pa->arrival - pb->arrival;
	if (!c) {
		c = pa->priority - pb->priority;
		if (!c)
			c = pa->burst - pb->burst;
	}
	return c ? c / abs(c) : 0;
}

int cmpjob(job *ja, job *jb)
{
	int c = ja->start - jb->start;
	if (!c) {
		if (ja->start == jb->start)
			c = ja->p->arrival - jb->p->arrival;
		if (!c) {
			c = ja->priority - jb->priority;
			if (!c)
				c = ja->length - jb->length;
		}
	}
	return c ? c / abs(c) : 0;
}

int cmplast(process *pa, process *pb)
{
	int c = pa->lastrun - pb->lastrun;
	return c ? c / abs(c) : 0;
}

int min(int a, int b) { return a < b ? a : b; }

double avg_response = 0, avg_waiting = 0, avg_turnaround = 0;
int response, waiting, turnaround, qt;
int numjobs = 0, ct = 0;

// process queue
node *head = NULL, *tail = NULL;

void enqueue(job *j, bool priority)
{
	node *n = (node *)malloc(sizeof(node));
	n->j = j;
	n->next = n->prev = NULL;
	if (head == NULL)
		head = tail = n;
	else {
		if (priority) {
			node *read = head;
			if (cmpjob(j, head->j) == -1) {
				head->prev = n;
				n->next = head;
				head = n;
			} else {
				while (read->next && cmpjob(j, read->j) >= 0)
					read = read->next;
				if (read->next == NULL && cmpjob(j, read->j) >= 0) {
					tail->next = n;
					n->prev = tail;
					tail = n;
				} else {
					read->prev->next = n;
					n->prev = read->prev;
					n->next = read;
					read->prev = n;
				}
			}
		} else {
			tail->next = n;
			n->prev = tail;
			tail = n;
		}
	}
	node *read = head;
	while (read) {
		printf("(s%d, L%d, p%d)->", read->j->start, read->j->length, read->j->p->pid);
		read = read->next;
	}
	printf("\n");
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
	free(curnode);
	if (isEmpty())
		tail = NULL;
	if (head)
		head->prev = NULL;
	return j;
}

void print(process list[], size_t t)
{
	for (int i = 0; i < t; i++)
		printf("%d %d %d %d\n", list[i].pid, list[i].burst, list[i].arrival, list[i].priority);
}

job *toJob(process *p)
{
	job *j = (job *)malloc(sizeof(job));
	j->length = p->burst;
	j->p = p;
	return j;
}

job *createJobs(process list[], int t, int maxjobs, bool rr)
{
	int amount, base_arrival, njobs = 0;
	job *jobs = (job *)malloc(sizeof(job) * maxjobs);
	for (int i = 0; i < t; i++) {
		amount = list[i].burst;
		base_arrival = list[i].arrival;
		job *jb;
		if (rr) {
			while (amount - qt >= 0) {
				jb = (job *)malloc(sizeof(job));
				jb->length = qt;
				jb->start = base_arrival;
				jb->p = &list[i];
				base_arrival += qt;
				jobs[njobs++] = *jb;
				amount -= qt;
			}
			if (amount) {
				jb = (job *)malloc(sizeof(job));
				jb->length = amount;
				jb->start = base_arrival;
				jb->p = &list[i];
				jobs[njobs++] = *jb;
			}
		} else {
			jb = (job *)malloc(sizeof(job));
			jb->length = amount;
			jb->start = base_arrival;
			jb->p = &list[i];
			jobs[njobs++] = *jb;
		}
	}
	return (job *)jobs;
}

void calculate(process list[], job *cjobs, int t, bool rr, bool postsort, bool preemptive, int maxjobs)
{
	job *jobs = cjobs ? cjobs : createJobs(list, t, maxjobs, rr);
	if (postsort)
		qsort(jobs, maxjobs, sizeof(job), (comp)cmpjob);
	for (int i = 0; i < maxjobs; i++)
		enqueue(&jobs[i], preemptive);
	// calculating metrics for each process, here worry nothing about time slice
	while (!isEmpty()) {
		job *jb = dequeue();
		process *p = jb->p;
		if (!p->firstrun) {
			p->firstrun = 1;
			p->lastrun = -1;
			p->response = ct - p->arrival;
		}
		if (p->lastrun == -1)
			p->waiting = ct - p->arrival;
		else
			p->waiting += ct - p->lastrun;
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
		if (list[i].arrival < minarrival)
			minarrival = list[i].arrival;
		i++;
	}
	for (i = 0; i < t; i++)
		list[i].arrival -= minarrival;
	if (!strcmp(argv[2], "FCFS")) {
		qsort(list, t, sizeof(process), (comp)cmpproc);
		for (i = 0; i < t; i++) {
			process *p = &list[i];
			p->response = p->waiting = ct - p->arrival;
			ct += p->burst;
			p->turnaround = ct - p->arrival;
			p->lastrun = ct;
		}
	} else if (!strcmp(argv[2], "SJF")) {
		qsort(list, t, sizeof(process), (comp)cmpproc);
		int min, k = 1;
		for (j = 0; j < t; j++) {
			ct += list[j].burst;
			min = list[k].burst;
			// find the shortest job coming within burst period of currently-running process
			for (i = j + 1; i < t; i++) {
				if (ct >= list[i].arrival && list[i].burst < min && list[i].priority < list[k].priority) {
					process temp = list[k];
					list[k] = list[i];
					list[i] = temp;
				}
			}
			k++;
		}
		ct = 0;
		calculate(list, NULL, t, false, false, false, t);
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
					smallest = i;
			}
			if (!list[smallest].firstrun) {
				list[smallest].response = ct - list[smallest].arrival;
				list[smallest].firstrun = 1;
			}
			if (--sburst[smallest] == 0) {
				count++;
				end = ct + 1;
				list[smallest].waiting = end - list[smallest].arrival - list[smallest].burst;
				list[smallest].turnaround = end - list[smallest].arrival;
				list[smallest].lastrun = end;
			}
		}
	} else if (qt = atoi(argv[2])) {
		qsort(list, t, sizeof(process), (comp)cmpproc);
		/*for (i = 0; i < t; i++)
			numjobs += (int)ceil((double)list[i].burst / qt);
		calculate(list, NULL, t, true, true, false, numjobs);*/
		int bound, k = 1;
		enqueue(toJob(&list[0]), false);
		while (!isEmpty()) {
			job *j = dequeue();
			if (!j->p->firstrun) {
				j->p->response = ct - j->p->arrival;
				j->p->lastrun = -1;
				j->p->firstrun = 1;
			}
			if (j->p->lastrun != -1)
				j->p->waiting += ct - j->p->lastrun;
			else
				j->p->waiting = ct - j->p->arrival;
			bound = ct + min(qt, j->length);
			while (k < t && list[k].arrival < bound)
				enqueue(toJob(&list[k++]), false);
			if (j->length > qt) {
				job *jb = toJob(j->p);
				jb->length = j->length - qt;
				enqueue(jb, false);
			}
			ct += min(qt, j->length);
			j->p->lastrun = ct;
			j->p->turnaround = ct - j->p->arrival;
		}
	} else {
		perror("Unrecognized or incompatible arguments");
		exit(EXIT_FAILURE);
	}
	qsort(list, t, sizeof(process), (comp)cmplast);
	for (i = 0; i < t; i++) {
		printf("%d %d %d\n", response = list[i].response, waiting = list[i].waiting, turnaround = list[i].turnaround);
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
