import java.util.Scanner;
import java.util.Iterator;
import java.util.ListIterator;
import java.util.LinkedList;

import java.io.*;


class Graph {

	int	s;
	int	t;
	int	n;
	int	m;
	Node	excess;		// list of nodes with excess preflow
	Node	node[];
	Edge	edge[];
	boolean print = false;

	void pr(Object... args) {
		if (print) {
			for (Object arg : args) {
				System.out.print(arg + " ");
			}
			System.out.println();  // Print a newline after all arguments are printed
		}
	}
	

	Graph(Node node[], Edge edge[])
	{
		this.node	= node;
		this.n		= node.length;
		this.edge	= edge;
		this.m		= edge.length;
	}

	void print_graph(){
		// Step 1: Print all nodes with their excess and height
		pr("Nodes:");
		for (Node node : this.node) {  // Assuming 'nodes' is a list or array of Node objects
			pr("Node " + node.i + " -> Excess: " + node.e + ", Height: " + node.h);
		}
	
		// Step 2: Print all edges with their flow and capacity
		pr("\nEdges:");
		for (Edge edge : this.edge) {  // Assuming 'edges' is a list or array of Edge objects
			pr("Edge from Node " + edge.u.i + " to Node " + edge.v.i +
							   " -> Flow: " + edge.f + " / Capacity: " + edge.c);
		}
	
		// Step 3: Print all nodes in the excess list
		pr("\nExcess list:");
		Node tmp = excess;  // Assuming 'excess' is the head of the excess linked list
		while (tmp != null) {
			pr("Node " + tmp.i);
			tmp = tmp.next;
		}
   }

	void enter_excess(Node u)
	{
		if (u != node[s] && u != node[t]) {
			if (excess != null){
				if (u==excess){
					assert false;
				}
			}
			pr(u.i + " Just enetered excess...");
			u.next = excess;
			excess = u;
		}
	}

	Node other(Edge a, Node u)
	{
		if (a.u == u)	
			return a.v;
		else
			return a.u;
	}

	void relabel(Node u)
	{
		pr("Relable: "+ u.i + " To: " + (u.h+1));
		u.h++;
	}

	void push(Node u, Node v, Edge a)
	{
		int	d;	/* remaining capacity of the edge. */


		pr("U with height: " + u.h + " And excess: " + u.e);
		pr("V with height: " + v.h + " And excess: " + v.e);
		if (u == a.u) {
			// Min of u.excess and (edge capacity - edge flow)
			d = Math.min(u.e, a.c - a.f);
			a.f += d;
		} else {
			// Min of u.excess and (edge capacity + edge flow)
			// Since flow is in other direction
			d = Math.min(u.e, a.c + a.f);
			a.f -= d;
		}

		pr("Pushed: "+ d + " From: " + u.i + " To: " + v.i);
		

		u.e -= d;
		v.e += d;
		assert d > 0 ;
		assert u.e >= 0 ;
		assert Math.abs(a.f) <= a.c;


		if(u.e > 0){
			enter_excess(u);
		}
		if(v.e == d){
			enter_excess(v);
		}
	}

	int preflow(int s, int t)
	{
		ListIterator<Edge>	iter;
		int			b;
		Edge			a;
		Node			u;
		Node			v;
		
		this.s = s;
		this.t = t;
		node[s].h = n;

		iter = node[s].adj.listIterator();
		while (iter.hasNext()) {
			a = iter.next();

			node[s].e += a.c;

			push(node[s], other(a, node[s]), a);
		}
		print_graph();

		while (excess != null) {
			pr("while excess yay");
			u = excess;
			v = null;
			a = null;
			excess = u.next;
			pr("U with height: " + u.h + " And excess: " + u.e);

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
				if (u.h > v.h && b * a.f < a.c){
					pr("BREAK");
					break;
				}
				else{
					pr("v is null");
					v = null;
				}
			}

			if (v != null){
				push(u, v, a);
			}
			else{
				relabel(u);
				enter_excess(u);
			}
		}

		return node[t].e;
	}
}

class Node {
	int	h;
	int	e;
	int	i;
	Node	next;
	LinkedList<Edge>	adj;

	Node(int i)
	{
		this.i = i;
		adj = new LinkedList<Edge>();
	}
}

class Edge {
	Node	u;
	Node	v;
	int	f;
	int	c;

	Edge(Node u, Node v, int c)
	{
		this.u = u;
		this.v = v;
		this.c = c;

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
		f = g.preflow(0, n-1);
		double	end = System.currentTimeMillis();
		System.out.println("t = " + (end - begin) / 1000.0 + " s");
		System.out.println("f = " + f);
	}
}
