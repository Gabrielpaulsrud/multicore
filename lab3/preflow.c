/* This is an implementation of the preflow-push algorithm, by
 * Goldberg and Tarjan, for the 2021 EDAN26 Multicore programming labs.
 *
 * It is intended to be as simple as possible to understand and is
 * not optimized in any way.
 *
 * You should NOT read everything for this course.
 *
 * Focus on what is most similar to the pseudo code, i.e., the functions
 * preflow, push, and relabel.
 *
 * Some things about C are explained which are useful for everyone  
 * for lab 3, and things you most likely want to skip have a warning 
 * saying it is only for the curious or really curious. 
 * That can safely be ignored since it is not part of this course.
 *
 * Compile and run with: make
 *
 * Enable prints by changing from 1 to 0 at PRINT below.
 *
 * Feel free to ask any questions about it on Discord 
 * at #lab0-preflow-push
 *
 * A variable or function declared with static is only visible from
 * within its file so it is a good practice to use in order to avoid
 * conflicts for names which need not be visible from other files.
 *
 */
 
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include <time.h> // Include this for time-related functions
#include "pthread_barrier.h"  



#define PRINT 1			/* enable/disable prints. */

/* the funny do-while next clearly performs one iteration of the loop.
 * if you are really curious about why there is a loop, please check
 * the course book about the C preprocessor where it is explained. it
 * is to avoid bugs and/or syntax errors in case you use the pr in an
 * if-statement without { }.
 *
 */

#if PRINT
#define pr(...)		do { fprintf(stderr, __VA_ARGS__); } while (0)
#else
#define pr(...)		/* no effect at all */
#endif

#define MIN(a,b)	(((a)<=(b))?(a):(b))

/* introduce names for some structs. a struct is like a class, except
 * it cannot be extended and has no member methods, and everything is
 * public.
 *
 * using typedef like this means we can avoid writing 'struct' in 
 * every declaration. no new type is introduded and only a shorter name.
 *
 */

typedef struct graph_t	graph_t;
typedef struct node_t	node_t;
typedef struct edge_t	edge_t;
typedef struct list_t	list_t;
typedef struct thread_data_t	thread_data_t;
typedef struct exces_queues_t exces_queues_t;

struct exces_queues_t {
	node_t* excess;
	pthread_mutex_t* lock;
};

struct thread_data_t {
    graph_t *g;
    int i;
	pthread_barrier_t* first_barrier;
	pthread_barrier_t* second_barrier;
	int* n_alive_threads;
	pthread_mutex_t* n_alive_threads_lock;
};

struct list_t {
	edge_t*		edge;
	list_t*		next;
};

struct node_t {
	int		h;	/* height.			*/
	int		e;	/* excess flow.			*/
	list_t*		edge;	/* adjacency list.		*/
	node_t*		next;	/* with excess preflow.		*/
	pthread_mutex_t lock;    /* Mutex to protect e. */
};

struct edge_t {
	node_t*		u;	/* one of the two nodes.	*/
	node_t*		v;	/* the other. 			*/
	int		f;	/* flow > 0 if from u to v.	*/
	int		c;	/* capacity.			*/
	pthread_mutex_t lock;    /* Mutex to protect f. */
};

struct graph_t {
	int		n;	/* nodes.			*/
	int		m;	/* edges.			*/
	node_t*		v;	/* array of n nodes.		*/
	edge_t*		e;	/* array of m edges.		*/
	node_t*		s;	/* source.			*/
	node_t*		t;	/* sink.			*/
	node_t*		excess;	/* nodes with e > 0 except s,t.	*/
	pthread_mutex_t excess_lock;    /* Mutex to protect f. */
	pthread_mutex_t meta_excess_mutex;
	pthread_cond_t excess_cond;
	int excess_lock_is_locked;
};

/* a remark about C arrays. the phrase above 'array of n nodes' is using
 * the word 'array' in a general sense for any language. in C an array
 * (i.e., the technical term array in ISO C) is declared as: int x[10],
 * i.e., with [size] but for convenience most people refer to the data
 * in memory as an array here despite the graph_t's v and e members 
 * are not strictly arrays. they are pointers. once we have allocated
 * memory for the data in the ''array'' for the pointer, the syntax of
 * using an array or pointer is the same so we can refer to a node with
 *
 * 			g->v[i]
 *
 * where the -> is identical to Java's . in this expression.
 * 
 * in summary: just use the v and e as arrays.
 * 
 * a difference between C and Java is that in Java you can really not
 * have an array of nodes as we do. instead you need to have an array
 * of node references. in C we can have both arrays and local variables
 * with structs that are not allocated as with Java's new but instead
 * as any basic type such as int.
 * 
 */

static char* progname;

#if PRINT

static int id(graph_t* g, node_t* v)
{
	/* return the node index for v.
	 *
	 * the rest is only for the curious.
	 *
	 * we convert a node pointer to its index by subtracting
	 * v and the array (which is a pointer) with all nodes.
	 *
	 * if p and q are pointers to elements of the same array,
	 * then p - q is the number of elements between p and q.
	 *
	 * we can of course also use q - p which is -(p - q)
	 *
	 * subtracting like this is only valid for pointers to the
	 * same array.
	 *
	 * what happens is a subtract instruction followed by a
	 * divide by the size of the array element.
	 *
	 */

	return v - g->v;
}
#endif

void error(const char* fmt, ...)
{
	/* print error message and exit. 
	 *
	 * it can be used as printf with formatting commands such as:
	 *
	 *	error("height is negative %d", v->h);
	 *
	 * the rest is only for the really curious. the va_list
	 * represents a compiler-specific type to handle an unknown
	 * number of arguments for this error function so that they
	 * can be passed to the vsprintf function that prints the
	 * error message to buf which is then printed to stderr.
	 *
	 * the compiler needs to keep track of which parameters are
	 * passed in integer registers, floating point registers, and
	 * which are instead written to the stack.
	 *
	 * avoid ... in performance critical code since it makes 
	 * life for optimizing compilers much more difficult. but in
	 * in error functions, they obviously are fine (unless we are
	 * sufficiently paranoid and don't want to risk an error 
	 * condition escalate and crash a car or nuclear reactor 		 
	 * instead of doing an even safer shutdown (corrupted memory
	 * can cause even more damage if we trust the stack is in good
	 * shape)).
	 *
	 */

	va_list		ap;
	char		buf[BUFSIZ];

	va_start(ap, fmt);
	vsprintf(buf, fmt, ap);

	if (progname != NULL)
		fprintf(stderr, "%s: ", progname);

	fprintf(stderr, "error: %s\n", buf);
	exit(1);
}

static int next_int()
{
        int     x;
        int     c;

	/* this is like Java's nextInt to get the next integer.
	 *
	 * we read the next integer one digit at a time which is
	 * simpler and faster than using the normal function
	 * fscanf that needs to do more work.
	 *
	 * we get the value of a digit character by subtracting '0'
	 * so the character '4' gives '4' - '0' == 4
	 *
	 * it works like this: say the next input is 124
	 * x is first 0, then 1, then 10 + 2, and then 120 + 4.
	 *
	 */

	x = 0;
        while (isdigit(c = getchar()))
                x = 10 * x + c - '0';

        return x;
}

static void* xmalloc(size_t s)
{
	void*		p;

	/* allocate s bytes from the heap and check that there was
	 * memory for our request.
	 *
	 * memory from malloc contains garbage except at the beginning
	 * of the program execution when it contains zeroes for 
	 * security reasons so that no program should read data written
	 * by a different program and user.
	 *
	 * size_t is an unsigned integer type (printed with %zu and
	 * not %d as for int).
	 *
	 */

	p = malloc(s);

	if (p == NULL)
		error("out of memory: malloc(%zu) failed", s);

	return p;
}

static void* xcalloc(size_t n, size_t s)
{
	void*		p;

	p = xmalloc(n * s);

	/* memset sets everything (in this case) to 0. */
	memset(p, 0, n * s);

	/* for the curious: so memset is equivalent to a simple
	 * loop but a call to memset needs less memory, and also
 	 * most computers have special instructions to zero cache 
	 * blocks which usually are used by memset since it normally
	 * is written in assembler code. note that good compilers 
	 * decide themselves whether to use memset or a for-loop
	 * so it often does not matter. for small amounts of memory
	 * such as a few bytes, good compilers will just use a 
	 * sequence of store instructions and no call or loop at all.
	 *
	 */

	return p;
}

static void add_edge(node_t* u, edge_t* e)
{
	list_t*		p;

	/* allocate memory for a list link and put it first
	 * in the adjacency list of u.
	 *
	 */

	p = xmalloc(sizeof(list_t));
	p->edge = e;
	p->next = u->edge;
	u->edge = p;
}

static void connect(node_t* u, node_t* v, int c, edge_t* e)
{
	/* connect two nodes by putting a shared (same object)
	 * in their adjacency lists.
	 *
	 */

	e->u = u;
	e->v = v;
	e->c = c;

	add_edge(u, e);
	add_edge(v, e);
}

static graph_t* new_graph(FILE* in, int n, int m)
{
	graph_t*	g;
	node_t*		u;
	node_t*		v;
	int		i;
	int		a;
	int		b;
	int		c;
	
	g = xmalloc(sizeof(graph_t));

	g->n = n;
	g->m = m;
	
	g->v = xcalloc(n, sizeof(node_t));
	g->e = xcalloc(m, sizeof(edge_t));

	// Initialize the pthread_mutex_t for each edge
    for (int i = 0; i < m; i++) {
        pthread_mutex_init(&g->e[i].lock, NULL);
    }

	// Initialize the pthread_mutex_t for each node
	for (int i = 0; i < n; i++) {
        pthread_mutex_init(&g->v[i].lock, NULL);
    }

	// Initialize the mutex for excess lock
	pthread_mutex_init(&g->excess_lock, NULL);
	pthread_mutex_init(&g->meta_excess_mutex, NULL);
	pthread_cond_init(&g->excess_cond, NULL);
	g->excess_lock_is_locked = 0;
	g->s = &g->v[0];
	g->t = &g->v[n-1];
	g->excess = NULL;

	for (i = 0; i < m; i += 1) {
		a = next_int();
		b = next_int();
		c = next_int();
		u = &g->v[a];
		v = &g->v[b];
		connect(u, v, c, g->e+i);
	}

	return g;
}

static void enter_excess(graph_t* g, node_t* v)
{
	/* put v at the front of the list of nodes
	 * that have excess preflow > 0.
	 *
	 * note that for the algorithm, this is just
	 * a set of nodes which has no order but putting it
	 * it first is simplest.
	 *
	 */
	pthread_mutex_lock(&g->meta_excess_mutex);
	
	if (v != g->t && v != g->s) {
		v->next = g->excess;
		g->excess = v;
	}
	pr("entred excess %d, g->excess is %p\n: ", id(g, v), (g->excess ? (void*)g->excess : "NULL"));
	
	// g->excess_lock_is_locked = 0;
	pthread_mutex_unlock(&g->meta_excess_mutex);
}

static node_t* leave_excess(graph_t* g)
{
	node_t*		v;

	/* take any node from the set of nodes with excess preflow
	 * and for simplicity we always take the first.
	 *
	 */

	pthread_mutex_lock(&g->meta_excess_mutex);

	v = g->excess;

	if (v != NULL)
		g->excess = v->next;
	if(v == NULL)
		pr("left excess: NULL\n");
	else
		pr("left excess: %d\n", id(g, v));


	// g->excess_lock_is_locked = 0;
	pthread_mutex_unlock(&g->meta_excess_mutex);

	return v;
}

static int excess_left(graph_t* g)
{
	node_t*		v;

	/* take any node from the set of nodes with excess preflow
	 * and for simplicity we always take the first.
	 *
	 */
	
	pthread_mutex_lock(&g->meta_excess_mutex);

	v = g->excess;

	pthread_mutex_unlock(&g->meta_excess_mutex);
	if(v == NULL){
		pr("left excess: NULL\n");
		return 0;}
	else {
		pr("left excess: %d\n", id(g, v));
		return 1;
	}
}


void wait_all_locks(pthread_mutex_t* lock1, pthread_mutex_t* lock2, pthread_mutex_t* lock3)
{
    int lock1_acquired = 0;
    int lock2_acquired = 0;
    int lock3_acquired = 0;

    while (1) {
		pr("WAIT ALL LOCKS\n");
        // Try to lock the first mutex
        if (pthread_mutex_trylock(lock1) == 0) {
            lock1_acquired = 1;
			// Try to lock the second mutex
			if (pthread_mutex_trylock(lock2) == 0) {
				lock2_acquired = 1;
				// Try to lock the third mutex
				if (pthread_mutex_trylock(lock3) == 0) {
					lock3_acquired = 1;
				}
			}
        }
		pr("GOT ALL LOCKS\n");

        // If all locks are acquired, break the loop
        if (lock1_acquired && lock2_acquired && lock3_acquired) {
            break;
        }

        // If any lock failed, release the ones that were successfully acquired
        if (lock1_acquired) {
            pthread_mutex_unlock(lock1);
            lock1_acquired = 0;
        }
        if (lock2_acquired) {
            pthread_mutex_unlock(lock2);
            lock2_acquired = 0;
        }
        if (lock3_acquired) {
            pthread_mutex_unlock(lock3);
            lock3_acquired = 0;
        }

        // Optionally: Add a small delay to avoid busy-waiting
        // usleep(10); // Sleep for 100 microseconds before retrying
    }
}

int alive_threads(int* alive_threads, pthread_mutex_t* alive_threads_lock) {
    // Lock the mutex before reading the value
    pthread_mutex_lock(alive_threads_lock);

    // Read the value of alive_threads_counter
    int n_alive_threads = *alive_threads;
	pr("Alive threads read as: %d\n", (*alive_threads));

    // Unlock the mutex after reading
    pthread_mutex_unlock(alive_threads_lock);

	return n_alive_threads;
}

void sub_alive_threads(int* alive_threads, pthread_mutex_t* alive_threads_lock) {
    // Lock the mutex before reading the value
    pthread_mutex_lock(alive_threads_lock);

    // Read the value of alive_threads_counter
    (*alive_threads)--;
	pr("Alive threads reduced to: %d\n", (*alive_threads));

    // Unlock the mutex after reading
    pthread_mutex_unlock(alive_threads_lock);
}

static void push(graph_t* g, node_t* u, node_t* v, edge_t* e)
{
	wait_all_locks(&e->u->lock, &e->v->lock, &e->lock);
	int		d;	/* remaining capacity of the edge. */

	pr("push from %d to %d: ", id(g, u), id(g, v));
	pr("f = %d, c = %d, so ", e->f, e->c);
	
	if (u == e->u) {
		// Min of u.excess and (edge capacity - edge flow)
		d = MIN(u->e, e->c - e->f);
		e->f += d;
	} else {
		// Min of u.excess and (edge capacity + edge flow)
		// Since flow is in other direction
		d = MIN(u->e, e->c + e->f);
		e->f -= d;
	}

	pr("pushing %d\n", d);

	u->e -= d;
	v->e += d;

	/* the following are always true. */

	assert(d >= 0);
	assert(u->e >= 0);
	assert(abs(e->f) <= e->c);

	if (u->e > 0) {

		/* still some remaining so let u push more. */

		enter_excess(g, u);
	}

	if (v->e == d) {

		/* since v has d excess now it had zero before and
		 * can now push.
		 *
		 */

		enter_excess(g, v);
	}

	// Once done, unlock all mutexes
    pthread_mutex_unlock(&u->lock);
    pthread_mutex_unlock(&v->lock);
    pthread_mutex_unlock(&e->lock);

	// fprintf(stderr, "thread ending\n");
}

static void relabel(graph_t* g, node_t* u)
{
	u->h += 1;

	pr("relabel %d now h = %d\n", id(g, u), u->h);

	enter_excess(g, u);
}

static node_t* other(node_t* u, edge_t* e)
{
	if (u == e->u)
		return e->v;
	else
		return e->u;
}
	
// Function to print the graph
void print_graph(graph_t* g) {
    // Print all nodes
    pr("Nodes:\n");
    for (int i = 0; i < g->n; i++) {
        node_t* node = &g->v[i];
        pr("Node %d: excess = %d, height = %d\n", id(g, node), node->e, node->h);
    }

    // Print all edges
    pr("\nEdges:\n");
    for (int i = 0; i < g->m; i++) {
        edge_t* edge = &g->e[i];
        pr("Edge between Node %d and Node %d: capacity = %d, flow = %d\n",
               id(g, edge->u), id(g, edge->v), edge->c, edge->f);
    }
}

void *_push_or_relabel(void* arg){
	thread_data_t* args = (thread_data_t*) arg;
	int i = args->i;
	graph_t* g = args->g;
	pthread_barrier_t* barrier = args->first_barrier;

	node_t* u;
	node_t* v;
	edge_t* e;
	int b; //Direction
	list_t*	p; //Adjecency list
	while ((u = leave_excess(g)) != NULL ){
		/* u is any node with excess preflow. */
	
		pr("[%d] Selected u = %d with h = %d and e = %d\n", i, id(g, u), u->h, u->e);

		/* if we can push we must push and only if we could
		* not push anything, we are allowed to relabel.
		*
		* we can push to multiple nodes if we wish but
		* here we just push once for simplicity.
		*
		*/

		v = NULL;
		p = u->edge; // p = first item of adjacency list

		while (p != NULL) {
			e = p->edge; // e = edge in current first item in p ()
			p = p->next; // change p to point to next item in adjecency list

			if (u == e->u) { //If selected node is u, set direction to positive
				v = e->v;
				b = 1;
			} else { // Else, set direction as negative
				v = e->u;
				b = -1;
			}
			// U is always selected, v is always other

			//Check if height of u is larger than v 
			// AND that directed excess flow is smaller than capacity
			// Why does second part matter?? Can't we send a subset of the possible flow
			// My guess is we will break cause we want to try push over current edge
			if (u->h > v->h && b * e->f < e->c) 
				break;
			else
				v = NULL;
		}
		pr("[%d] Waiting for barrier\n", i);
		pthread_barrier_wait(barrier);
		if (v != NULL) {
			pr("Starting thread in main\n");
			push(g, u, v, e);

		}
		else {
			relabel(g, u);
		}
	}
	// free(args);
	pr("[%d] Decided to die\n", i);
	sub_alive_threads(args->n_alive_threads, args->n_alive_threads_lock);
	do {
		pr("[%d] Dead wait\n", i);
		pthread_barrier_wait(barrier);
	} while (alive_threads(args->n_alive_threads, args->n_alive_threads_lock));
	pr("[%d] Finaly over\n", i);
}

void *__push_or_relabel(void* arg){
	thread_data_t* args = (thread_data_t*) arg;
	int i = args->i;
	graph_t* g = args->g;
	pthread_barrier_t* first_barrier = args->first_barrier;
	pthread_barrier_t* second_barrier = args->second_barrier;
	int* n_alive_threads = args->n_alive_threads;
	pthread_mutex_t* n_alive_threads_lock = args->n_alive_threads_lock;

	node_t* u;
	node_t* v;
	edge_t* e;
	int b; //Direction
	list_t*	p; //Adjecency list
	int has_reported_dead = 0;
	while ((u = leave_excess(g)) != NULL){
		/* u is any node with excess preflow. */
		if (u == NULL){
			// if (!has_reported_dead){
			// 	pr("[%d] Reported dead\n", i);
				// sub_alive_threads(args->n_alive_threads, args->n_alive_threads_lock);
			// }
			pr("[%d] Dead wait\n", i);
			pthread_barrier_wait(first_barrier);
		}
		else {
			pr("[%d] Selected u = %d with h = %d and e = %d\n", i, id(g, u), u->h, u->e);

			/* if we can push we must push and only if we could
			* not push anything, we are allowed to relabel.
			*
			* we can push to multiple nodes if we wish but
			* here we just push once for simplicity.
			*
			*/

			v = NULL;
			p = u->edge; // p = first item of adjacency list

			while (p != NULL) {
				e = p->edge; // e = edge in current first item in p ()
				p = p->next; // change p to point to next item in adjecency list

				if (u == e->u) { //If selected node is u, set direction to positive
					v = e->v;
					b = 1;
				} else { // Else, set direction as negative
					v = e->u;
					b = -1;
				}
				// U is always selected, v is always other

				//Check if height of u is larger than v 
				// AND that directed excess flow is smaller than capacity
				// Why does second part matter?? Can't we send a subset of the possible flow
				// My guess is we will break cause we want to try push over current edge
				if (u->h > v->h && b * e->f < e->c) 
					break;
				else
					v = NULL;
			}
			pr("[%d] Waiting for first barrier\n", i);
			pthread_barrier_wait(first_barrier);
			if (v != NULL) {
				pr("Starting thread in main\n");
				push(g, u, v, e);

			}
			else {
				relabel(g, u);
			}
		}
		// ADD A SECOND BARRIER!!!!!
		pr("[%d] Waiting for second barrier\n", i);
		pthread_barrier_wait(second_barrier);
	}
	// free(args);
	// pr("[%d] Decided to die\n", i);
	// sub_alive_threads(args->n_alive_threads, args->n_alive_threads_lock);
	// do {
	// 	pr("[%d] Dead wait\n", i);
	// 	pthread_barrier_wait(barrier);
	// } while (alive_threads(args->n_alive_threads, args->n_alive_threads_lock));
	pr("[%d] Finaly over\n", i);
}

void *push_or_relabel(void* arg){
	thread_data_t* args = (thread_data_t*) arg;
	int i = args->i;
	graph_t* g = args->g;
	pthread_barrier_t* first_barrier = args->first_barrier;
	pthread_barrier_t* second_barrier = args->second_barrier;
	int* n_alive_threads = args->n_alive_threads;
	pthread_mutex_t* n_alive_threads_lock = args->n_alive_threads_lock;

	node_t* u;
	node_t* v;
	edge_t* e;
	int b; //Direction
	list_t*	p; //Adjecency list
	int has_reported_dead = 0;
	while ((u = leave_excess(g)) != NULL){
		/* u is any node with excess preflow. */
		
		pr("[%d] Selected u = %d with h = %d and e = %d\n", i, id(g, u), u->h, u->e);

		/* if we can push we must push and only if we could
		* not push anything, we are allowed to relabel.
		*
		* we can push to multiple nodes if we wish but
		* here we just push once for simplicity.
		*
		*/

		v = NULL;
		p = u->edge; // p = first item of adjacency list

		while (p != NULL) {
			e = p->edge; // e = edge in current first item in p ()
			p = p->next; // change p to point to next item in adjecency list

			if (u == e->u) { //If selected node is u, set direction to positive
				v = e->v;
				b = 1;
			} else { // Else, set direction as negative
				v = e->u;
				b = -1;
			}
			// U is always selected, v is always other

			//Check if height of u is larger than v 
			// AND that directed excess flow is smaller than capacity
			// Why does second part matter?? Can't we send a subset of the possible flow
			// My guess is we will break cause we want to try push over current edge
			if (u->h > v->h && b * e->f < e->c) 
				break;
			else
				v = NULL;
		}
		pr("[%d] Waiting for first barrier\n", i);
		pthread_barrier_wait(first_barrier);
		if (v != NULL) {
			pr("Starting thread in main\n");
			push(g, u, v, e);

		}
		else {
			relabel(g, u);
		}
		pr("[%d] Waiting for second barrier\n", i);
		pthread_barrier_wait(second_barrier);
	}	
	// free(args);
	pr("[%d] Decided to die\n", i);
	pr("[%d] First dead wait\n", i);
	pthread_barrier_wait(first_barrier);
	sub_alive_threads(n_alive_threads, n_alive_threads_lock);
	pr("[%d] Second dead wait\n", i);
	pthread_barrier_wait(second_barrier);
	pr("[%d] Going to die\n", i);
	while (alive_threads(n_alive_threads, n_alive_threads_lock)) {
		pr("[%d] First dead wait\n", i);
		pthread_barrier_wait(first_barrier);
		pr("[%d] Second dead wait\n", i);
		pthread_barrier_wait(second_barrier);
	}
	pr("[%d] Finaly over\n", i);
}



int preflow(graph_t* g, int n_threads)
{
	node_t*		s;
	node_t*		u;
	node_t*		v;
	edge_t*		e;
	list_t*		p;
	int		b;

	s = g->s;
	s->h = g->n;

	p = s->edge;

	// Copy init from Lab 1
	while (p != NULL) {
		e = p->edge;
		p = p->next;

		s->e += e->c;
		push(g, s, other(s, e), e);
	}

	// Create n new threads for this:
    pthread_t threads[n_threads]; // Array to hold thread identifiers
    thread_data_t thread_args[n_threads]; // Array to hold arguments for each thread

	//Keep track of alive threads
	int n_alive_threads = n_threads;
	pthread_mutex_t n_alive_threads_lock;
	pthread_mutex_init(&n_alive_threads_lock, NULL);

	pthread_barrier_t first_barrier;
	pthread_barrierattr_t first_barrier_attr;
	pthread_barrierattr_init(&first_barrier_attr);

	pthread_barrier_init(&first_barrier, &first_barrier_attr, n_threads);

	pthread_barrier_t second_barrier;
	pthread_barrierattr_t second_barrier_attr;
	pthread_barrierattr_init(&second_barrier_attr);

	pthread_barrier_init(&second_barrier, &second_barrier_attr, n_threads);

	// Start all threads
	for(int i = 0; i < n_threads; i++){
		thread_args[i].g = g; // Pass pointer to graph structure
        thread_args[i].i = i;  // Pass index to thread
		thread_args[i].first_barrier = &first_barrier;
		thread_args[i].second_barrier = &second_barrier;
		thread_args[i].n_alive_threads = &n_alive_threads;
		thread_args[i].n_alive_threads_lock = &n_alive_threads_lock;


        int rc = pthread_create(&threads[i], NULL, push_or_relabel, &thread_args[i]);
		if (rc) {
            printf("Error:unable to create thread, %d\n", rc);
            exit(-1);
        }
	}

	// Joing the n threads
	for (int i = 0; i < n_threads; i++) {
        int rc = pthread_join(threads[i], NULL);
        if (rc) {
            fprintf(stderr, "Error joining thread %d: %d\n", i, rc);
			exit(-1);
		    // Handle the error as needed
        }
    }
	pthread_barrier_destroy(&first_barrier);
	pthread_barrier_destroy(&second_barrier);
	//return the answer 
	return g->t->e;
}



static void free_graph(graph_t* g)
{
	int		i;
	list_t*		p;
	list_t*		q;

	// Clean up: destroy mutexes and free memory
    for (int i = 0; i < g->m; i++) {
		
        // pthread_mutex_lock(&g->e[i].lock);
        pthread_mutex_destroy(&g->e[i].lock);
		// pr("freed lock\n");
    }
	 for (int i = 0; i < g->n; i++) {
        // pthread_mutex_destroy(&g->v[i].lock);
        pthread_mutex_destroy(&g->v[i].lock);
		// pr("freed lock\n");
    }

	pthread_mutex_destroy(&g->excess_lock);
	pthread_mutex_destroy(&g->meta_excess_mutex);
	pthread_cond_destroy(&g->excess_cond);

	for (i = 0; i < g->n; i += 1) {
		p = g->v[i].edge;
		while (p != NULL) {
			q = p->next;
			free(p);
			p = q;
		}
	}
	free(g->v);
	free(g->e);
	free(g);
}

int main(int argc, char* argv[])
{
	FILE*		in;	/* input file set to stdin	*/
	graph_t*	g;	/* undirected graph. 		*/
	int		f;	/* output from preflow.		*/
	int		n;	/* number of nodes.		*/
	int		m;	/* number of edges.		*/

	progname = argv[0];	/* name is a string in argv[0]. */
	int n_threads;
	if (argc > 1) {
        n_threads = atoi(argv[1]);  // Convert the first argument to an int
        printf("n_threads: %d\n", n_threads);
    }
	else {
		n_threads = 2;
	}

	in = stdin;		/* same as System.in in Java.	*/

	n = next_int();
	m = next_int();

	/* skip C and P from the 6railwayplanning lab in EDAF05 */
	next_int();
	next_int();

	g = new_graph(in, n, m);

	fclose(in);

	f = preflow(g, n_threads);

	printf("f = %d\n", f);

	free_graph(g);

	return 0;
}
