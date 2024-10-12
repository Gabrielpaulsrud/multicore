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
// typedef struct work_queue_t work_queue_t;
typedef struct dynamic_list_t dynamic_list_t;
typedef enum Instruction Instruction;
typedef struct node_instruction_t node_instruction_t;
typedef struct instruction_queue_t instruction_queue_t;
typedef struct excess_queue_t excess_queue_t;

enum Instruction {
  PUSH,
  RELABEL,
};

struct node_instruction_t{
	node_t* u;
	Instruction instuction; //0=push, 1=
};

struct instruction_queue_t
{
	node_instruction_t** queue;
	int current_index;
	int size;
	//TODO: make resizeable
};


// struct work_queue_t {
// 	node_t* _Atomic excess;
// 	pthread_mutex_t*  lock;
// };

struct excess_queue_t {
	node_t** queue;
	int current_index;
	int size;
};


struct thread_data_t {
    graph_t *g;
    int i;
	pthread_barrier_t* first_barrier;
	pthread_barrier_t* second_barrier;
	pthread_mutex_t* n_alive_threads_lock;
	// work_queue_t** work_queues;
	excess_queue_t** excess_queues;
	instruction_queue_t** instruction_queues;
	int n_threads;
	int* is_alive;
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
	atomic_int accumulated_push; /* Accumulated push in atomic */
	int in_excess; //TODO, make non atomic??
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
	 * it can be used as // // printf with formatting commands such as:
	 *
	 *	error("height is negative %d", v->h);
	 *
	 * the rest is only for the really curious. the va_list
	 * represents a compiler-specific type to handle an unknown
	 * number of arguments for this error function so that they
	 * can be passed to the vs// // printf function that prints the
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

instruction_queue_t* init_queue(int size){
	instruction_queue_t* iq = (instruction_queue_t*)xmalloc(sizeof(instruction_queue_t));
	iq->queue = (node_instruction_t**)xmalloc(size * sizeof(node_instruction_t*));
	iq->current_index = 0;
	iq->size=size;
	return iq;
}

void add_to_queue(instruction_queue_t* iq, node_t* node, Instruction inst){
	if (iq->current_index >= iq->size) {
        assert(0); // Queue is full, can't add more elements.
    }
	node_instruction_t* node_inst = (node_instruction_t*)xmalloc(sizeof(node_instruction_t));
	node_inst->instuction = inst;
	node_inst->u = node;
	iq->queue[iq->current_index++] = node_inst;
}

node_instruction_t* pop_from_queue(instruction_queue_t* iq) {
    if (iq->current_index <= 0) {
        return NULL; // Queue is empty
    }

    // Decrease the index first to point to the last element
    node_instruction_t* last_instruction = iq->queue[--iq->current_index];
    return last_instruction; // Return the last node
}

// Function to reset the index of the queue
void reset_queue(instruction_queue_t* pq) {
    pq->current_index = 0; // Reset index to 0
}

// Cleanup function to free memory (optional but good practice)
void free_queue(instruction_queue_t* pq) {
    free(pq->queue); // Free the queue array
    free(pq); // Free the queue struct
}

excess_queue_t* init_excess_queue(int size){
	excess_queue_t* eq = (excess_queue_t*)xmalloc(sizeof(excess_queue_t));
	eq->queue = malloc(size * sizeof(node_t *));
	eq->current_index = 0;
	eq->size=size;
	return eq;
}

void add_to_excess_queue(excess_queue_t* eq, node_t* node){
	if (eq->current_index >= eq->size) {
        assert(0); // Queue is full, can't add more elements.
    }
	if (node->in_excess){
		return;
	}
	eq->queue[eq->current_index] = node;
	eq->current_index++;
	node->in_excess = 1;
}

node_t* pop_from_excess_queue(excess_queue_t* eq) {
    if (eq->current_index <= 0) {
        return NULL; // Queue is empty
    }

    // Decrease the index first to point to the last element
    node_t* last_node = eq->queue[--eq->current_index];
	last_node->in_excess = 0;
    return last_node; // Return the last node
}

// Function to reset the index of the queue
void reset_excess_queue(excess_queue_t* pq) {
    pq->current_index = 0; // Reset index to 0
}

// Cleanup function to free memory (optional but good practice)
void free_excess_queue(excess_queue_t* pq) {
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

static void add_push_instruction(graph_t* g, node_t* u, node_t* v, edge_t* e, instruction_queue_t* instruction_queue)
{
	int	d = 0;	/* remaining capacity of the edge. */

	pr("push from %d to %d: ", id(g, u), id(g, v));
	pr("f = %d, c = %d, so \n", e->f, e->c);
	
	if (u == e->u) {
		// Min of u.excess and (edge capacity - edge flow)
		d = MIN(u->e+u->accumulated_push, e->c - e->f);
		e->f+=d;
	} else {
		// Min of u.excess and (edge capacity + edge flow)
		// Since flow is in other direction
		d = MIN(u->e+u->accumulated_push, e->c + e->f);
		e->f-=d;
	}

	pr("pushing %d\n", d);

	// u->accumulated_push -= d;	//MEMORD
	atomic_fetch_sub_explicit(&u->accumulated_push, d, memory_order_relaxed);

	// v->accumulated_push += d; //MEMORD
	atomic_fetch_add_explicit(&v->accumulated_push, d, memory_order_relaxed);

	add_to_queue(instruction_queue, u, PUSH);
	add_to_queue(instruction_queue, v, PUSH);

	/* the following are always true. */
	assert(d >= 0);
	assert(u->e + atomic_load_explicit(&u->accumulated_push, memory_order_relaxed) >= 0); //MEMORD. AND DO CORRCTLY
	assert(e->f <= e->c);

}

static void push_atomic_node(graph_t* g, node_t* u, excess_queue_t* eq){
	// pr("execute_atomic_node %d, %d\n", u->accumulated_push, u->e);
	// if (u->accumulated_push == 0){
	// 	return;
	// }
	int accumulated_push_value = atomic_exchange_explicit(&u->accumulated_push, 0, memory_order_relaxed);
	u->e += accumulated_push_value;

	// u->in_excess = 0;
	if (u->e > 0){
		// enter_excess(g, u, work_queue);
		add_to_excess_queue(eq, u);
	}
	// pr("result was: %d\n", u->e);
}

static void relabel_atomic_node(graph_t* g, node_t* u, excess_queue_t* eq)
{
	// pthread_mutex_lock(&u->lock);
	u->h += 1;
	// pthread_mutex_unlock(&u->lock);
	pr("relabel (%d) now h = %d\n", id(g, u), u->h);
	add_to_excess_queue(eq, u);
	// enter_excess(g, u, work_queue);
}

static int execute_atomic_nodes(graph_t* g, excess_queue_t** eqs, int n_threads, instruction_queue_t** instruction_queue, node_t* source, node_t* zink, int n_nodes){
	// Loop over each node and execute it
    // for (int i = 0; i < g->n; i++) {
    //     execute_atomic_node(g, &g->v[i], work_queues[next_queue_ind%n_threads]);
	// 	(next_queue_ind)++;
    // }
	node_instruction_t* inst;
	node_t* excess_nodes[n_nodes];
	int excess_index = 0;
	for(int i = 0; i<n_threads; i++){
		while((inst=pop_from_queue(instruction_queue[i]))!=NULL){
			node_t* u = inst->u;
			int b = u->h;
			pr("h");
			pr("%d\n", b);
			pr("u: %p\n", u);
			if (inst->instuction == PUSH){ // Push
				// // printf("push %d %d\n", id(g, u), u->accumulated_push);
				pr("Popped instruction %d\n", inst->instuction);
				// push_atomic_node(g, inst->u, work_queues[next_queue_ind%n_threads]);
				// execute_atomic_node(g, u, work_queues[next_queue_ind%n_threads]);
				// int accumulated_push_value = atomic_load_explicit(&inst->u->accumulated_push, memory_order_relaxed);
				// atomic_store_explicit(&inst->u->accumulated_push, 0, memory_order_relaxed);
				int accumulated_push_value = atomic_exchange_explicit(&inst->u->accumulated_push, 0, memory_order_relaxed);
				u->e += accumulated_push_value;
				if (u->e > 0){
					excess_nodes[excess_index] = u;
					excess_index++;
				}
			}
			else{ //relabel
				// // printf("relabel %d\n", id(g, u));
				u->h += 1;
				excess_nodes[excess_index] = u;
				excess_index++;
			}
			// free(inst);
		}
	}
	if (excess_index == 0){
		pr("ret 0");
		return 0;
	}
	for (;excess_index>0; excess_index--){
		if (excess_nodes[excess_index-1] != source && excess_nodes[excess_index-1] != zink){
			add_to_excess_queue(eqs[excess_index%n_threads], excess_nodes[excess_index-1]);
		}
	}
	pr("ret 1");
	return 1;
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
    // for (int i = 0; i <= g->n; i++) {
    //     node_t* node = &g->v[i];
	// 	if (node->e){
    //     	pr("Node %d: excess = %d, height = %d, in excess = %d\n", id(g, node), node->e, node->h, node->in_excess);
	// 	}
	// }
    pr("Nodes:\n");
    for (int i = 0; i < g->n; i++) {
        node_t* node = &g->v[i];
        pr("Node %d: excess = %d, height = %d, in excess = %d\n", id(g, node), node->e, node->h, node->in_excess);
    }

    // Print all edges
    pr("\nEdges:\n");
    for (int i = 0; i < g->m; i++) {
        edge_t* edge = &g->e[i];
        pr("Edge between Node %d and Node %d: capacity = %d, flow = %d\n",
               id(g, edge->u), id(g, edge->v), edge->c, edge->f);
    }
}

void *push_or_relabel(void* arg){
	thread_data_t* args = (thread_data_t*) arg;
	int i = args->i;
	graph_t* g = args->g;
	pthread_barrier_t* first_barrier = args->first_barrier;
	pthread_barrier_t* second_barrier = args->second_barrier;
	pthread_mutex_t* n_alive_threads_lock = args->n_alive_threads_lock;
	instruction_queue_t** instruction_queues = args->instruction_queues;
	excess_queue_t** eqs = args->excess_queues;
	int n_threads = args->n_threads;
	int next_queue_index = i;
	int* is_alive = args->is_alive;


	node_t* u;
	node_t* v;
	edge_t* e;
	int b; //Direction
	list_t*	p; //Adjecency list
	// while ((u = leave_excess(g, work_queues[i])) != NULL){
	while(*is_alive){
		// // printf("start while\n");
		// sleep(1);
		// usleep(100);
		/* u is any node with excess preflow. */
		// if ((u = leave_excess(g, work_queues[i])) != NULL){
		while ((u = pop_from_excess_queue(eqs[i])) != NULL){
			pr("u: %p\n", u);
			pr("[%d] Selected u = (%d) with h = %d and e = %d\n", i, id(g, u), u->h, u->e);
			// // printf("[%d] Selected u = (%d) with h = %d and e = %d\n", i, id(g, u), u->h, u->e);

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
				pr("[%d] took a p\n", i);
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
					// // // printf("height of %d larger, adding push instructions", id(g, u));
				else
					v = NULL;
			}
			if (v == NULL) {
				add_to_queue(instruction_queues[i], u, RELABEL);
			}
			else {
				add_push_instruction(g, u, v, e, instruction_queues[i]);
			}
		}
		pr("[%d] Waiting for first barrier\n", i);
		// Calculate new pushes accumulatively in atomic variable
		pthread_barrier_wait(first_barrier);
		if (i == 0){
			*is_alive = execute_atomic_nodes(g, eqs, n_threads, instruction_queues, g->s, g->t, g->n);
		}
		pthread_barrier_wait(second_barrier);
	}	
	pr("[%d] Finaly over\n", i);
	return NULL;
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
	excess_queue_t* eqs[n_threads];
	for (int i = 0; i<n_threads; i++){
		eqs[i] = init_excess_queue((g->n)*2);
	}

	mode_t** excess_nodes[n_threads];
	int j = 0;
	while (p != NULL) {
		e = p->edge;
		p = p->next;

		node_t* receive_node = other(s, e);
		s->e -= e->c;
		receive_node->e = e->c;
		e->f = e->c;

		add_to_excess_queue(eqs[j%n_threads], receive_node);
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

	instruction_queue_t** iqs = malloc(n_threads * sizeof(instruction_queue_t*));
	for (int i = 0; i<n_threads; i++){
		iqs[i] = init_queue(5000); //What is appropirate size;
	}
	
	int is_alive = 1;

	// Start all threads
	for(int i = 0; i < n_threads; i++){
		// // printf("created thread\n");
		thread_args[i].g = g; // Pass pointer to graph structure
        thread_args[i].i = i;  // Pass index to thread
		thread_args[i].first_barrier = &first_barrier;
		thread_args[i].second_barrier = &second_barrier;
		thread_args[i].excess_queues = eqs;
		thread_args[i].n_threads = n_threads;
		thread_args[i].instruction_queues = iqs;
		thread_args[i].is_alive = &is_alive;

        int rc = pthread_create(&threads[i], NULL, push_or_relabel, &thread_args[i]);
		if (rc) {
            pr("Error:unable to create thread, %d\n", rc);
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
	for (int i = 0; i<n_threads; i++){
		free_queue(iqs[i]);
	}
	free(iqs);

	//return the answer 
	return g->t->e;
}



static void free_graph(graph_t* g)
{
	int		i;
	list_t*		p;
	list_t*		q;

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
        pr("n_threads: %d\n", n_threads);
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
