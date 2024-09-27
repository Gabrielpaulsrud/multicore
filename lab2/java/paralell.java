import java.util.Scanner;
import java.util.Iterator;
import java.util.ListIterator;
import java.util.LinkedList;
import java.util.List;
import java.io.*;

class Print {
    static boolean print = false;

	static void mby(Object... args) {
		if (print) {
			for (Object arg : args) {
				System.out.print(arg + " ");
			}
			System.out.println();  // Print a newline after all arguments are printed
		}
	}
}

class PushThread extends BasePushThread {
	
	static boolean is_done = false;

	public PushThread(Node s, Node t, int i){
	this.s = s;
	this.t = t;
	this.i = i;
	}

	@Override
	void push(Node u, Node v, Edge a){
		increaseOngoingPushes();
		super.push(u, v, a);
		decreaseOngoingPushes();
	}

	@Override
	public void run() {
		while (get_excess() != null || getOngoingPushes() != 0) {
			//while (get_excess() != null ){
			Print.mby("[" + this.i + "] Excess at start: "+ excess_string());
			v = null;
			a = null;
			Node u = leave_excess();
			if (u == null){
				Print.mby("LEAVE NULL");
				break;
			}
			
			Print.mby("[" + this.i + "] "+ "U with index: (" + u.i + ") height: " + u.height() + " And excess: " + u.excess());

			iter = u.adj.listIterator();
			while (iter.hasNext()) {
				a = iter.next();
				if (u == a.u){ //If selected node is u, set direction to positive
					v = a.v;
					b = 1;
				}
				else{ // Else, set direction as negative
					v = a.u;
					b = -1;
				}
				if (u.height() > v.height() && b * a.flow() < a.c){
					// Print.mby("BREAK");
					break;
				}
				else{
					// Print.mby("v is null");
					v = null;
				}
			}

			if (v != null){
				push(u, v, a);
			}
			else{
				u.relabel();
				enter_excess(u);
			}
		}
		Print.mby("[" + this.i + "] "+ "End while");
		synchronized (BasePushThread.class){
			BasePushThread.class.notifyAll();
		}
	}
}

class InitialPushThread extends BasePushThread {

	public InitialPushThread(Node s, Node t, int i){
		this.i = i;
		this.s = s;
		this.t = t;
	}

	@Override
	public void run(){
		iter = s.adj.listIterator();
		while (iter.hasNext()) {
			a = iter.next();
			s.e += a.c;
			push(s, other(a, s), a);
		}
	}
}

class BasePushThread extends Thread {
	static Node excess; //mby should be static? 
	static int n_ongoing_pushes;
	Node s;
	Node t;
	ListIterator<Edge>	iter;
	int b; //Direction
	int i; //Index

	
	Node v;
	Edge a;

	@Override
	public void run(){
		assert false; //This class is not intended to be ran itself.
	}

	private static final Object nThreadsLock = new Object();

	void increaseOngoingPushes() {
    	synchronized (nThreadsLock) {
			// Print.mby("[" + this.i + "] " + "Increased it");
        	n_ongoing_pushes++;
    	}
	}

	void decreaseOngoingPushes() {
		synchronized (nThreadsLock) {
			// Print.mby("[" + this.i + "] " + "Decreased it");
			n_ongoing_pushes--;
		}
	}

	int getOngoingPushes() {
		synchronized (nThreadsLock) {
			return n_ongoing_pushes;
		}
	}

	void enter_excess(Node u){
		synchronized (BasePushThread.class){
			if (u != s && u != t) {
				if (excess != null){
					if (u==excess){
						assert false;
					}
				}
					Print.mby("[" + this.i + "] "+ u.i + " Just enetered excess...");
					u.next = excess;
					excess = u;
					BasePushThread.class.notify();
				}
			}
		}

	Node get_excess(){
		Node current_excess;
		synchronized (BasePushThread.class){
			current_excess = excess;
		}
		return current_excess;

	}

	String excess_string(){
		synchronized (BasePushThread.class){
			String printstring = "";
			Node current = excess;
			while (current != null) {
				printstring = printstring + " " + current.i;
				current = current.next;  // Move to the next node
			}
			return printstring;
		}
	}

	Node leave_excess(){
		Node leaver;
		synchronized (BasePushThread.class){
			while (excess == null) {
				try {
					Print.mby("[" + this.i + "] "+ "..." + getOngoingPushes());
					if (getOngoingPushes() == 0){
						return null;
					}
					BasePushThread.class.wait();
				} catch (InterruptedException e) {
					Print.mby("JEEES");
					e.printStackTrace();
					assert false;
				}
			}
			leaver = excess;
			excess = leaver.next;
		}
		return leaver;
	}

	Node other(Edge a, Node u)
	{
		if (a.u == u)	
			return a.v;
		else
			return a.u;
	}

	// void relabel(Node u)
	// {
	// 	Print.mby("Relable: "+ u.i + " To: " + (u.h+1));
	// 	u.h++;
	// }

	void push(Node u, Node v, Edge a)
	{
		int	d;	/* remaining capacity of the edge. */

		Print.mby("[" + this.i + "] "+ "In Push");
		Print.mby("[" + this.i + "] "+ "U with index: (" + u.i + ") height: " + u.height() + " And excess: " + u.excess());
		Print.mby("[" + this.i + "] "+ "V with index: (" + v.i + ") height: " + v.height() + " And excess: " + v.excess());
		if (u == a.u) {
			// Min of u.excess and (edge capacity - edge flow)
			d = Math.min(u.excess(), a.c - a.flow());
			a.changeFlow(d);
		} else {
			// Min of u.excess and (edge capacity + edge flow)
			// Since flow is in other direction
			d = Math.min(u.excess(), a.c + a.flow());
			a.changeFlow(-d);
		}

		Print.mby("[" + this.i + "] "+ "Pushed: "+ d + " From: " + u.i + " To: " + v.i);
		
		synchronized (u.lock) {
			u.changeExcess(-d);
			if(u.excess() > 0){
				enter_excess(u);
			}
		}

		synchronized (v.lock) {
			v.changeExcess(d);
			if(v.excess() == d){
				enter_excess(v);
			}
		}
		
		assert d > 0 ;
		assert u.e >= 0 ;
		assert Math.abs(a.flow()) <= a.c;

		
		
		Print.mby("[" + this.i + "] " + "Excess after push: " + excess_string());
	}
}


class Graph {

	int	s;
	int	t;
	int	n;
	int	m;
	Node	excess;		// list of nodes with excess preflow
	Node	node[];
	Edge	edge[];
	boolean print = true;

	

	Graph(Node node[], Edge edge[])
	{
		this.node	= node;
		this.n		= node.length;
		this.edge	= edge;
		this.m		= edge.length;
	}

	void print_graph(){
		// Step 1: Print all nodes with their excess and height
		Print.mby("Nodes:");
		for (Node node : this.node) {  // Assuming 'nodes' is a list or array of Node objects
			Print.mby("Node " + node.i + " -> Excess: " + node.e + ", Height: " + node.h);
		}
	
		// Step 2: Print all edges with their flow and capacity
		Print.mby("\nEdges:");
		for (Edge edge : this.edge) {  // Assuming 'edges' is a list or array of Edge objects
			Print.mby("Edge from Node " + edge.u.i + " to Node " + edge.v.i +
							   " -> Flow: " + edge.f + " / Capacity: " + edge.c);
		}
	
		// Step 3: Print all nodes in the excess list
		Print.mby("\nExcess list:");
		Node tmp = excess;  // Assuming 'excess' is the head of the excess linked list
		while (tmp != null) {
			Print.mby("Node " + tmp.i);
			tmp = tmp.next;
		}
   }


	int preflow(int s, int t, int n_threads)
	{
		ListIterator<Edge>	iter;
		int			b;
		Edge			a;
		Node			u;
		Node			v;
		
		this.s = s;
		this.t = t;
		node[s].h = n;
		Node excess = null;
		BasePushThread.excess = excess;
		InitialPushThread init = new InitialPushThread(node[s], node[t], -1);
		init.start();
		try {
			init.join();
		} catch (InterruptedException e) {
			Print.mby("JEEES 2");
			e.printStackTrace();
			assert false;
		}
		PushThread.n_ongoing_pushes = 0;
		PushThread[] pushThreads = new PushThread[n_threads];
        for (int i = 0; i < n_threads; ++i){
			pushThreads[i] = new PushThread(node[s], node[t], i);
		}
		for (int i = 0; i < n_threads; ++i){
			pushThreads[i].start();
		}
		for (int i = 0; i < n_threads; ++i){
			try {
				pushThreads[i].join();
			} catch (InterruptedException e) {
				Print.mby("JEEES 3");
				e.printStackTrace();
				assert false;
			}
		}
		print_graph();

		// spawn threasd + wait ofr threads

		return node[t].e;
	}
}

class Node {
	int	h;
	int	e;
	int	i;
	Node	next;
	LinkedList<Edge>	adj;
	final Object lock = new Object();

	Node(int i)
	{
		this.i = i;
		adj = new LinkedList<Edge>();
	}

	void relabel()
	{
		synchronized (this.lock) {
			Print.mby("Relable: "+ i + " To: " + (h+1));
			h++;
		}
	}

	int height()
	{
		// Print.mby("Relable: "+ i + " To: " + (h+1));
		synchronized (this.lock) {
			return h;
		}
	}

	void changeExcess(int increase)
	{
		// Print.mby("Set: "+ i + " excess to: " + (e+increase));
		synchronized (this.lock) 
			{e = e + increase;
		}
	}

	int excess()
	{
		// Print.mby("Read: "+ i + " excess as: " + e);
		synchronized (this.lock) {			
			return e;
		}
	}
}

class Edge {
	Node	u;
	Node	v;
	int	f;
	int	c;
	final Object lock = new Object();


	Edge(Node u, Node v, int c)
	{
		this.u = u;
		this.v = v;
		this.c = c;

	}

	void changeFlow(int increase)
	{
		// Print.mby("Set: " + u.i + " -> " + v.i + " flow to: " + (f+increase));
		synchronized (this.lock){
		f = f + increase;}
	}

	int flow(){
		// Print.mby("Read: " + u.i + " -> " + v.i + " flow as: " + f);
		synchronized (this.lock){
			return f;
		}
	}
}

class Preflow {
	public static void main(String args[])
	{
		double	begin = System.currentTimeMillis();
		Scanner s = new Scanner(System.in);
		int	n;
		int	m;
		int	i;
		int	u;
		int	v;
		int	c;
		int	f;
		Graph	g;

		n = s.nextInt();
		m = s.nextInt();
		s.nextInt();
		s.nextInt();
		Node[] node = new Node[n];
		Edge[] edge = new Edge[m];

		for (i = 0; i < n; i += 1)
			node[i] = new Node(i);

		for (i = 0; i < m; i += 1) {
			u = s.nextInt();
			v = s.nextInt();
			c = s.nextInt(); 
			edge[i] = new Edge(node[u], node[v], c);
			node[u].adj.addLast(edge[i]);
			node[v].adj.addLast(edge[i]);
		}

		g = new Graph(node, edge);
		f = g.preflow(0, n-1, 2);
		double	end = System.currentTimeMillis();
		System.out.println("t = " + (end - begin) / 1000.0 + " s");
		System.out.println("f = " + f);
	}
}
