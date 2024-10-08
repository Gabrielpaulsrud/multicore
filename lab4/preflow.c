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
#include <unistd.h>
#include <time.h> // Include this for time-related functions
#include "pthread_barrier.h"
#include <stdatomic.h>
#include <unistd.h>



#define PRINT 0			/* enable/disable prints. */

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
typedef struct excess_queue_t excess_queue_t;
typedef struct dynamic_list_t dynamic_list_t;
typedef struct push_queue_t push_queue_t;

struct excess_queue_t {
	node_t* _Atomic excess;
	pthread_mutex_t* lock;
};

struct push_queue_t
{
	node_t** queue;
	atomic_int current_index;
	int size;
};


struct thread_data_t {
    graph_t *g;
    int i;
	pthread_barrier_t* first_barrier;
	pthread_barrier_t* second_barrier;
	pthread_barrier_t* third_barrier;
	atomic_int* n_alive_threads;
	pthread_mutex_t* n_alive_threads_lock;
	excess_queue_t** excess_queue;
	push_queue_t* push_queue;
	int n_threads;
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
	// pthread_mutex_t lock;    /* Mutex to protect e. */
	atomic_int accumulated_push; /* Accumulated push in atomic */
	atomic_int in_excess; //TODO, make non atomic??
	atomic_int in_push_queue;
};

struct edge_t {
	node_t*		u;	/* one of the two nodes.	*/
	node_t*		v;	/* the other. 			*/
	int		f;	/* flow > 0 if from u to v.	*/
	int		c;	/* capacity.			*/
};

struct graph_t {
	int		n;	/* nodes.			*/
	int		m;	/* edges.			*/
	node_t*		v;	/* array of n nodes.		*/
	edge_t*		e;	/* array of m edges.		*/
	node_t*		s;	/* source.			*/
	node_t*		t;	/* sink.			*/
	node_t*		excess;	/* nodes with e > 0 except s,t.	*/
	// pthread_mutex_t excess_lock;    /* Mutex to protect f. */
	// pthread_mutex_t meta_excess_mutex;
	// pthread_cond_t excess_cond;
	// int excess_lock_is_locked;
};

struct dynamic_list_t {
    node_t** nodes;
    size_t size;
    size_t capacity;
} ;

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

// #if PRINT

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
// #endif

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


// Function to initialize the queue with a given size
push_queue_t* init_queue(int size) {
    push_queue_t* pq = (push_queue_t*)xmalloc(sizeof(push_queue_t));

    pq->queue = (node_t**)xmalloc(size * sizeof(node_t*));

    pq->current_index = 0;
    pq->size = size; // Set the size of the queue
    return pq;
}

// Function to add an element to the queue
void add_to_queue(push_queue_t* pq, node_t* node) {
    if (pq->current_index >= pq->size) {
        assert(0); // Queue is full, can't add more elements.
    }
    // if (!node->in_push_queue){
    // if (!atomic_load_explicit(&node->in_push_queue, memory_order_relaxed)){
	// 	atomic_store_explicit(&node->in_push_queue, 1, memory_order_relaxed);
    // 	pq->queue[pq->current_index++] = node; // Add node and increment index
	// }
	int expected = 0; // Value we expect to find
	if (atomic_compare_exchange_strong(&node->in_push_queue, &expected, 1)) {
		// Only one thread will enter this block
		pq->queue[pq->current_index++] = node; // Add node and increment index
	}
}

node_t* pop_from_queue(push_queue_t* pq) {
    if (pq->current_index <= 0) {
        return NULL; // Queue is empty
    }

    // Decrease the index first to point to the last element
    node_t* last_node = pq->queue[--pq->current_index];
	// last_node->in_push_queue=0;
	atomic_store_explicit(&last_node->in_push_queue, 0, memory_order_relaxed);
	
    return last_node; // Return the last node
}

// Function to reset the index of the queue
void reset_queue(push_queue_t* pq) {
    pq->current_index = 0; // Reset index to 0
}

// Cleanup function to free memory (optional but good practice)
void free_queue(push_queue_t* pq) {
    free(pq->queue); // Free the queue array
    free(pq); // Free the queue struct
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
    // for (int i = 0; i < m; i++) {
    //     pthread_mutex_init(&g->e[i].lock, NULL);
    // }

	// Initialize the pthread_mutex_t for each node
	for (int i = 0; i < n; i++) {
        // pthread_mutex_init(&g->v[i].lock, NULL);
		g->v[i].in_excess = 0;
		g->v[i].in_push_queue = 0;
    }

	// Initialize the mutex for excess lock
	// pthread_mutex_init(&g->excess_lock, NULL);
	// pthread_mutex_init(&g->meta_excess_mutex, NULL);
	// pthread_cond_init(&g->excess_cond, NULL);
	// g->excess_lock_is_locked = 0;
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

static void enter_excess(graph_t* g, node_t* v, excess_queue_t* excess_queue)
{
	/* put v at the front of the list of nodes
	 * that have excess preflow > 0.
	 *
	 * note that for the algorithm, this is just
	 * a set of nodes which has no order but putting it
	 * it first is simplest.
	 *
	 */
	
	// pthread_mutex_lock(excess_queue->lock);
	// if (atomic_load_explicit(&v->in_excess, memory_order_relaxed)){
	// 	pr("HEHE");
	// 	// pthread_mutex_unlock(excess_queue->lock);
	// 	return;
	// }
	int expected = 0;
	if (!atomic_compare_exchange_strong(&v->in_excess, &expected, 1)){
		return;
	}

	if (v != g->t && v != g->s) {
		// pthread_mutex_lock(excess_queue->lock);
		// v->next = excess_queue->excess;
		// excess_queue->excess = v;
		v->next = atomic_exchange(&excess_queue->excess, v);
		// pthread_mutex_unlock(excess_queue->lock);
		// v->in_excess = 1;
	}
	
	pr("entred excess (%d), g->excess is %p\n: ", id(g, v), (g->excess ? (void*)g->excess : "NULL"));
	
	// g->excess_lock_is_locked = 0;
	// pthread_mutex_unlock(excess_queue->lock);
}

static node_t* leave_excess(graph_t* g, excess_queue_t* excess_queue)
{
	// node_t*		v;

	/* take any node from the set of nodes with excess preflow
	 * and for simplicity we always take the first.
	 *
	 */

	// pthread_mutex_lock(excess_queue->lock);
			// v = excess_queue->excess;

			// if (v != NULL){
			// 	v->in_excess=0;
			// 	excess_queue->excess = v->next;
			// }
	// node_t *v = atomic_exchange(&excess_queue->excess, NULL);

	// // Check if the old value was not NULL
	// if (v != NULL) {
	// 	v->in_excess = 0; // Perform operations on v
	// 	atomic_store(&excess_queue->excess, v->next); // Atomically update excess to v->next
	// }
	node_t *v = atomic_load(&excess_queue->excess);
	while (v != NULL) {
		// Try to update excess_queue->excess to v->next only if it is still v
		if (atomic_compare_exchange_weak(&excess_queue->excess, &v, v->next)) {
			v->in_excess = 0; // Modify v after ensuring the atomic exchange
			break;
		}
		v = atomic_load(&excess_queue->excess); // Reload in case of failure
	}

	if(v == NULL)
		pr("left excess: NULL\n");
	else
		pr("left excess: %d\n", id(g, v));

	
	// pthread_mutex_unlock(excess_queue->lock);

	return v;
}

static int excess_left(graph_t* g, excess_queue_t* excess_queue)
{
	node_t*		v;

	/* take any node from the set of nodes with excess preflow
	 * and for simplicity we always take the first.
	 *
	 */
	
	// pthread_mutex_lock(excess_queue->lock);

	v = g->excess;

	// pthread_mutex_unlock(excess_queue->lock);
	if(v == NULL){
		pr("left excess: NULL\n");
		return 0;}
	else {
		pr("left excess: %d\n", id(g, v));
		return 1;
	}
}

int alive_threads(atomic_int* alive_threads) {
    // Lock the mutex before reading the value
    // pthread_mutex_lock(alive_threads_lock);

    // Read the value of alive_threads_counter
    // int n_alive_threads = *alive_threads;
	int n_alive_threads = atomic_load_explicit(alive_threads, memory_order_relaxed);
	pr("Alive threads read as: %d\n", (*alive_threads));

    // Unlock the mutex after reading
    // pthread_mutex_unlock(alive_threads_lock);

	return n_alive_threads;
}

void sub_alive_threads(atomic_int* alive_threads) {
    // Lock the mutex before reading the value
    // pthread_mutex_lock(alive_threads_lock);

    // Read the value of alive_threads_counter
    atomic_fetch_sub_explicit(alive_threads, 1, memory_order_relaxed);
	pr("Alive threads reduced to: %d\n", (*alive_threads));

    // Unlock the mutex after reading
    // pthread_mutex_unlock(alive_threads_lock);
}

void add_alive_threads(atomic_int* alive_threads) {
    // Lock the mutex before reading the value
    // pthread_mutex_lock(alive_threads_lock);

    // Read the value of alive_threads_counter
    // (*alive_threads)++;
	atomic_fetch_add_explicit(alive_threads, 1, memory_order_relaxed);
	pr("Alive threads increased to: %d\n", (*alive_threads));

    // Unlock the mutex after reading
    // pthread_mutex_unlock(alive_threads_lock);
}

static void push_to_atomic(graph_t* g, node_t* u, node_t* v, edge_t* e, push_queue_t* push_queue)
{
	int	d = 0;	/* remaining capacity of the edge. */

	pr("push from %d to %d: ", id(g, u), id(g, v));
	pr("f = %d, c = %d, so \n", e->f, e->c);
	
	if (u == e->u) {
		// Min of u.excess and (edge capacity - edge flow)
		d = MIN(u->e, e->c - e->f);
		e->f+=d;
	} else {
		// Min of u.excess and (edge capacity + edge flow)
		// Since flow is in other direction
		d = MIN(u->e, e->c + e->f);
		e->f-=d;
	}

	pr("pushing %d\n", d);

	// u->accumulated_push -= d;	//MEMORD
	atomic_fetch_sub_explicit(&u->accumulated_push, d, memory_order_relaxed);

	// v->accumulated_push += d; //MEMORD
	atomic_fetch_add_explicit(&v->accumulated_push, d, memory_order_relaxed);

	add_to_queue(push_queue, u);
	add_to_queue(push_queue, v);

	/* the following are always true. */
	// assert(d >= 0);
	// assert(u->e + atomic_load_explicit(&u->accumulated_push, memory_order_relaxed) >= 0); //MEMORD. AND DO CORRCTLY
	// assert(e->f <= e->c);

}

static void execute_atomic_node(graph_t* g, node_t* u, excess_queue_t* excess_queue){
	// pr("execute_atomic_node %d, %d\n", u->accumulated_push, u->e);
	// if (u->accumulated_push == 0){
	// 	return;
	// }
	int accumulated_push_value = atomic_exchange_explicit(&u->accumulated_push, 0, memory_order_relaxed);
	u->e += accumulated_push_value;

	// u->in_excess = 0;
	if (u->e > 0){
		enter_excess(g, u, excess_queue);
	}
	// pr("result was: %d\n", u->e);
}

static void execute_atomic_nodes(graph_t* g, excess_queue_t** excess_queues, int n_threads, int next_queue_ind, push_queue_t* push_queue){
	// Loop over each node and execute it
    // for (int i = 0; i < g->n; i++) {
    //     execute_atomic_node(g, &g->v[i], excess_queues[next_queue_ind%n_threads]);
	// 	(next_queue_ind)++;
    // }
	node_t* u;
	while((u=(pop_from_queue(push_queue)))!= NULL){
		execute_atomic_node(g, u, excess_queues[next_queue_ind%n_threads]);
		(next_queue_ind)++;
	}
}


static void relabel(graph_t* g, node_t* u, excess_queue_t* excess_queue)
{
	// pthread_mutex_lock(&u->lock);
	u->h += 1;
	// pthread_mutex_unlock(&u->lock);
	pr("relabel (%d) now h = %d\n", id(g, u), u->h);

	enter_excess(g, u, excess_queue);
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
		if (node->e){
        	printf("Node %d: excess = %d, height = %d, in excess = %d\n", id(g, node), node->e, node->h, node->in_excess);
		}
	}
    // pr("Nodes:\n");
    // for (int i = 0; i < g->n; i++) {
    //     node_t* node = &g->v[i];
    //     pr("Node %d: excess = %d, height = %d, in excess = %d\n", id(g, node), node->e, node->h, node->in_excess);
    // }

    // Print all edges
    // pr("\nEdges:\n");
    // for (int i = 0; i < g->m; i++) {
    //     edge_t* edge = &g->e[i];
    //     pr("Edge between Node %d and Node %d: capacity = %d, flow = %d\n",
    //            id(g, edge->u), id(g, edge->v), edge->c, edge->f);
    // }
}

void *push_or_relabel(void* arg){
	thread_data_t* args = (thread_data_t*) arg;
	int i = args->i;
	graph_t* g = args->g;
	pthread_barrier_t* first_barrier = args->first_barrier;
	pthread_barrier_t* second_barrier = args->second_barrier;
	pthread_barrier_t* third_barrier = args->third_barrier;
	atomic_int *n_alive_threads = args->n_alive_threads;
	pthread_mutex_t* n_alive_threads_lock = args->n_alive_threads_lock;
	excess_queue_t** excess_queues = args->excess_queue;
	int n_threads = args->n_threads;
	int next_queue_index = i;
	int is_alive = 1;
	push_queue_t* push_queue = args->push_queue;


	node_t* u;
	node_t* v;
	edge_t* e;
	int b; //Direction
	list_t*	p; //Adjecency list
	// while ((u = leave_excess(g, excess_queues[i])) != NULL){
	while(alive_threads(n_alive_threads)){
		pr("start while\n");
		// sleep(1);
		// usleep(100);
		/* u is any node with excess preflow. */
		if ((u = leave_excess(g, excess_queues[i])) != NULL){
			if (!is_alive){
				is_alive = 1;
				pr("[%d] Decided to rescurect\n", i);
				add_alive_threads(n_alive_threads);
			}
			pr("[%d] Selected u = (%d) with h = %d and e = %d\n", i, id(g, u), u->h, u->e);

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
				int height_u;
				// pthread_mutex_lock(&u->lock);
				height_u = u->h;
				// pthread_mutex_unlock(&u->lock);
				int height_v;
				// pthread_mutex_lock(&v->lock);
				height_v = v->h;
				// pthread_mutex_unlock(&v->lock);

				if (height_u > height_v && b * e->f < e->c) 
					break;
				else
					v = NULL;
			}
			if (v!=NULL){
				push_to_atomic(g, u, v, e, push_queue);
			}
			pr("[%d] Waiting for first barrier\n", i);
			// Calculate new pushes accumulatively in atomic variable
			pthread_barrier_wait(first_barrier);
			if (i == 0){
				execute_atomic_nodes(g, excess_queues, n_threads, next_queue_index, push_queue);
			}
			pthread_barrier_wait(second_barrier);
			if (v == NULL) {
				relabel(g, u, excess_queues[next_queue_index%n_threads]);
			}
			next_queue_index++;
			// pr("[%d] Waiting for second barrier\n", i);
			pthread_barrier_wait(third_barrier);
		}
		else{
			// pr("[%d] First dead wait\n", i);
			pthread_barrier_wait(first_barrier);
			// for (int j = 1; j<=g->n; j++){
			// 	if (i == j%(n_threads)){
			// 		printf("[%d] handling node: %d\n", i, j);
			// 		if (&g->v[j-1] != g->s && &g->v[j-1] != g->t){
			// 			execute_atomic_node(g, &g->v[j-1], excess_queues[next_queue_index%n_threads]);
			// 		}
			// 	}
			// }
			if (i == 0){
				execute_atomic_nodes(g, excess_queues, n_threads, next_queue_index, push_queue);
				next_queue_index+=1;
			}
			if (is_alive){
				is_alive = 0;
				pr("[%d] Decided to die\n", i);
				sub_alive_threads(n_alive_threads);
			}
			// pr("[%d] Second dead wait\n", i);
			pthread_barrier_wait(second_barrier);
			pthread_barrier_wait(third_barrier);
		}
		if(i==0){
			// printf("full loop\n");
		}
	}	
	// free(args);
	
	
	// pr("[%d] Going to die\n", i);
	// while (alive_threads(n_alive_threads, n_alive_threads_lock)) {
	// 	pr("[%d] First dead wait\n", i);
	// 	pthread_barrier_wait(first_barrier);
	// 	pr("[%d] Second dead wait\n", i);
	// 	pthread_barrier_wait(second_barrier);
	// }
	pr("[%d] Finaly over\n", i);
	return NULL;
}

static void push(graph_t* g, node_t* u, node_t* v, edge_t* e, excess_queue_t* excess_queue)
{
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

	// assert(d >= 0);
	// assert(u->e >= 0);
	// assert(abs(e->f) <= e->c);

	if (u->e > 0) {

		/* still some remaining so let u push more. */

		enter_excess(g, u, excess_queue);
	}

	if (v->e == d) {

		/* since v has d excess now it had zero before and
		 * can now push.
		 *
		 */

		enter_excess(g, v, excess_queue);
	}

	// Once done, unlock all mutexes
    // pthread_mutex_unlock(&u->lock);
    // pthread_mutex_unlock(&v->lock);

	// fprintf(stderr, "thread ending\n");
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

	//Keep track of alive threads
	atomic_int n_alive_threads = n_threads;
	pthread_mutex_t n_alive_threads_lock;
	pthread_mutex_init(&n_alive_threads_lock, NULL);

	excess_queue_t *excess_queue[n_threads];
	//Initialize the excess_queues
	for (int k=0; k<n_threads; k++){
		excess_queue_t *queue = xmalloc(sizeof(excess_queue_t));
		excess_queue[k] = queue;


		excess_queue[k]->excess = NULL;
		queue->lock = xmalloc(sizeof(pthread_mutex_t));
		pthread_mutex_init(queue->lock, NULL);
	}
	// Copy init from Lab 1
	int j = 0;
	while (p != NULL) {
		e = p->edge;
		p = p->next;

		s->e += e->c;
		push(g, s, other(s, e), e, excess_queue[j%n_threads]);
		j++;
	}

	// Create n new threads for this:
    pthread_t threads[n_threads]; // Array to hold thread identifiers
    thread_data_t thread_args[n_threads]; // Array to hold arguments for each thread

	pthread_barrier_t first_barrier;
	pthread_barrierattr_t first_barrier_attr;
	pthread_barrierattr_init(&first_barrier_attr);

	pthread_barrier_init(&first_barrier, &first_barrier_attr, n_threads);

	pthread_barrier_t second_barrier;
	pthread_barrierattr_t second_barrier_attr;
	pthread_barrierattr_init(&second_barrier_attr);

	pthread_barrier_init(&second_barrier, &second_barrier_attr, n_threads);

	pthread_barrier_t third_barrier;
	pthread_barrierattr_t third_barrier_attr;
	pthread_barrierattr_init(&third_barrier_attr);

	pthread_barrier_init(&third_barrier, &third_barrier_attr, n_threads);
	push_queue_t* push_queue = init_queue(g->n);

	// Start all threads
	for(int i = 0; i < n_threads; i++){
		thread_args[i].g = g; // Pass pointer to graph structure
        thread_args[i].i = i;  // Pass index to thread
		thread_args[i].first_barrier = &first_barrier;
		thread_args[i].second_barrier = &second_barrier;
		thread_args[i].third_barrier = &third_barrier;
		thread_args[i].n_alive_threads = &n_alive_threads;
		thread_args[i].n_alive_threads_lock = &n_alive_threads_lock;
		thread_args[i].excess_queue = excess_queue;
		thread_args[i].n_threads = n_threads;
		thread_args[i].push_queue = push_queue;

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
	pthread_barrier_destroy(&third_barrier);
	free_queue(push_queue);
	for (int k=0; k<n_threads; k++){
		pthread_mutex_destroy(excess_queue[k]->lock);
	}
	//return the answer 
	return g->t->e;
}



static void free_graph(graph_t* g)
{
	int		i;
	list_t*		p;
	list_t*		q;

	// Clean up: destroy mutexes and free memory
    // for (int i = 0; i < g->m; i++) {
		
    //     // pthread_mutex_lock(&g->e[i].lock);
    //     pthread_mutex_destroy(&g->e[i].lock);
	// 	// pr("freed lock\n");
    // }
	 for (int i = 0; i < g->n; i++) {
        // pthread_mutex_destroy(&g->v[i].lock);
        // pthread_mutex_destroy(&g->v[i].lock);
		// pr("freed lock\n");
    }

	// pthread_mutex_destroy(&g->excess_lock);
	// pthread_mutex_destroy(&g->meta_excess_mutex);
	// pthread_cond_destroy(&g->excess_cond);

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
		n_threads = 5;
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
	print_graph(g);

	printf("f = %d\n", f);

	free_graph(g);

	return 0;
}
