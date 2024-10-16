#[macro_use] extern crate text_io;

use std::sync::{Mutex,Arc};
use std::collections::LinkedList;
use std::cmp;
use std::collections::VecDeque;
use std::thread;
use log::{info, debug};


enum LogLevel {
    Debug,
    Info,
}

#[derive(Copy, Clone)]
struct Node {
	i:	usize,			/* index of itself for debugging.	*/
	e:	i32,			/* excess preflow.			*/
	h:	i32,			/* height.				*/
}

#[derive(Copy, Clone)]
struct Edge {
        u_i:      usize,  
        v_i:      usize,
        f:      i32,
        c:      i32,
}

impl Node {
	fn new(ii:usize) -> Node {
		Node { i: ii, e: 0, h: 0 }
	}
}

impl Edge {
        fn new(uu:usize, vv:usize,cc:i32) -> Edge {
                Edge { u_i: uu, v_i: vv, f: 0, c: cc }      
        }
}

fn print_nodes(nodes: &Vec<Arc<Mutex<Node>>>, log_level: LogLevel) {
    for node in nodes.iter() {
        // Lock the node to get read access to its data
        let node = node.lock().unwrap();
        
		match log_level {
			LogLevel::Debug => {debug!("[{}], Excess: {}, Height: {}", node.i, node.e, node.h);}
			LogLevel::Info => {info!("[{}], Excess: {}, Height: {}", node.i, node.e, node.h);}
		}
    }
}

fn print_edges(edges: &Vec<Arc<Mutex<Edge>>>, log_level: LogLevel) {
	match log_level {
		LogLevel::Debug => {
			for edge in edges.iter() {
				let edge = edge.lock().unwrap();
				debug!("({} -> {}) with flow: {}/{}", edge.u_i, edge.v_i, edge.f, edge.c);
			}
		}
		LogLevel::Info => {
			for edge in edges.iter() {
				let edge = edge.lock().unwrap();
				info!("({} -> {}) with flow: {}/{}", edge.u_i, edge.v_i, edge.f, edge.c);
			}
		}
	}
	
}

fn print_adj(adj: &Vec<LinkedList<usize>>, log_level: LogLevel) {
    for (node_index, edges) in adj.iter().enumerate() {
		match log_level {
			LogLevel::Debug => {debug!("Node {}: {}", node_index, edges.iter().map(|edge_index| edge_index.to_string()).collect::<Vec<_>>().join(" "));}
			LogLevel::Info => {info!("Node {}: {}", node_index, edges.iter().map(|edge_index| edge_index.to_string()).collect::<Vec<_>>().join(" "));}
    	}
	}
}

fn relabel(nodes: &Vec<Arc<Mutex<Node>>>, u_i: usize, excess:& Arc<Mutex<VecDeque<usize>>>, i: usize){
	let mut u = nodes[u_i].lock().unwrap();
	u.h +=1;
	debug!("[{}] wait excess relabel", i);
	let mut excess = excess.lock().unwrap();
	excess.push_back(u_i);
	debug!("[{}] Relabel: {} to {}", i, u_i, u.h);
}

fn push(u_i: usize, v_i: usize, nodes: &Vec<Arc<Mutex<Node>>>, excess:& Arc<Mutex<VecDeque<usize>>>, edge: & Arc<Mutex<Edge>>, s_i: &usize, t_i: &usize, i: usize){
	debug!("[{}] Should push {} -> {}", i, u_i, v_i);
	let mut edge = edge.lock().unwrap();
	let mut u = nodes[u_i].lock().unwrap();
	let mut v = nodes[v_i].lock().unwrap();
	
	let d: i32;
	if u_i == edge.u_i {
		d = cmp::min(u.e, edge.c - edge.f);
		edge.f += d;
	} else {
		d = cmp::min(u.e, edge.c + edge.f);
		edge.f -= d;
	}
	u.e -= d;
	v.e += d;
	if u.e > 0 && u_i!= *s_i && u_i != *t_i{
		debug!("R1 Adding {} to excess", u_i);
		let mut excess = excess.lock().unwrap();
		excess.push_back(u_i);
	}
	if v.e == d && v_i!= *s_i && v_i != *t_i{
		debug!("R2 Adding {} to excess", v_i);
		let mut excess = excess.lock().unwrap();
		excess.push_back(v_i);
	}
}

fn get_next_excess(excess: &Arc<Mutex<VecDeque<usize>>>) -> Option<usize> {
    let mut excess = excess.lock().unwrap();
    excess.pop_front()
}

fn push_or_relabel(nodes: &Vec<Arc<Mutex<Node>>>, edges: &Vec<Arc<Mutex<Edge>>>,excess:& Arc<Mutex<VecDeque<usize>>>, adj: &Vec<LinkedList<usize>>, s_i: &usize, t_i: &usize, i: usize){
	while let Some(u_i) = get_next_excess(&excess){
		debug!("[{}] Start of loop", i);
		let edge_indexes = adj[u_i].iter();
		let mut v_found: bool = false;
		let mut v_i: usize = 0;
		let mut b: i32;
		let mut last_edge_index: usize = 0;
		for edge_index in edge_indexes{
			debug!("[{}] Wait for edge-index {}", i, *edge_index);
			let edge_copy: Edge;
			{
				let edge: std::sync::MutexGuard<'_, Edge> = edges[*edge_index].lock().unwrap();
				edge_copy = *edge;

				// let edge_address = &*edge as *const Edge;
				// let edge_copy_address = &edge_copy as *const Edge;
			
				// assert_ne!(edge_address, edge_copy_address, "edge and edge_copy point to the same address!");
				// assert!(false);
			}
			debug!("[{}] {} -> {}. Edge-index {}", i,  edge_copy.u_i, edge_copy.v_i, *edge_index);
			
			// let u: std::sync::MutexGuard<'_, Node>;
			// let v: std::sync::MutexGuard<'_, Node>;
			
			let edge_u_copy: Node;
			{
				debug!("[{}] Wait for u {}", i, edge_copy.u_i);
				let edge_u = nodes[edge_copy.u_i].lock().unwrap();
				edge_u_copy = *edge_u;
			}
			let edge_v_copy: Node;
			{
				debug!("[{}] Wait for v {}", i, edge_copy.v_i);
				let edge_v: std::sync::MutexGuard<'_, Node> = nodes[edge_copy.v_i].lock().unwrap();
				edge_v_copy = *edge_v;
			}
			let v: Node;
			let u: Node;
			if u_i==edge_copy.u_i {
				v_i = edge_copy.v_i;
				b = 1;
				u = edge_u_copy;
				v = edge_v_copy;

			}
			else {
				v_i = edge_copy.u_i;
				b = -1;
				u = edge_v_copy;
				v = edge_u_copy;
			}
			
			if u.h > v.h && b*edge_copy.f < edge_copy.c{
				v_found = true;
				last_edge_index = *edge_index;
				break;
			}
		}
		if v_found{
			debug!("[{}] Edge-index {}", i, last_edge_index);
			let edge = &edges[last_edge_index];
			push(u_i, v_i, &nodes, excess, &edge, &s_i, &t_i, i);
		}
		else {
			relabel(&nodes, u_i, excess, i);
		}
	}
}

fn main() {
	env_logger::init();
	let n: usize = read!();		/* n nodes.						*/
	let m: usize = read!();		/* m edges.						*/
	let _c: usize = read!();	/* underscore avoids warning about an unused variable.	*/
	let _p: usize = read!();	/* c and p are in the input from 6railwayplanning.	*/
	let mut nodes: Vec<Arc<Mutex<Node>>> = vec![];
	let mut edges: Vec<Arc<Mutex<Edge>>> = vec![];
	let mut adj: Vec<LinkedList<usize>> =Vec::with_capacity(n);
	let excess: Arc<Mutex<VecDeque<_>>> = Arc::new(Mutex::new(VecDeque::new()));

	let s_i = 0;
	let t_i = n-1;

	debug!("n = {}", n);
	debug!("m = {}", m);

	for i in 0..n {
		let u:Node = Node::new(i);
		nodes.push(Arc::new(Mutex::new(u))); 
		adj.push(LinkedList::new());
	}

	for i in 0..m {
		let u: usize = read!();
		let v: usize = read!();
		let c: i32 = read!();
		let e:Edge = Edge::new(u,v,c);
		adj[u].push_back(i);
		adj[v].push_back(i);
		edges.push(Arc::new(Mutex::new(e))); 
	}

	{
		let mut start_node = nodes[s_i].lock().unwrap();
		start_node.h = n as i32;
	}
	
	
	debug!("initial pushes");
	let iter = adj[s_i].iter();
	for edj in iter{
		let mut excess = excess.lock().unwrap();
		let mut edge = edges[*edj].lock().unwrap();
		debug!("pushed {}. {} -> {}", edge.c, edge.u_i, edge.v_i);
		
		// push(u_i, v_i, &nodes, &excess, edge, &s_i, &t_i);
		edge.f = edge.c;
		if edge.u_i == s_i {
			let mut v = nodes[edge.v_i].lock().unwrap();
			v.e = edge.c;
			if edge.v_i != t_i && edge.c > 0{
				excess.push_back(edge.v_i);
			}
		}
		else {
			let mut u = nodes[edge.u_i].lock().unwrap();
			u.e = edge.c;
			if edge.u_i != t_i && edge.c > 0{
				excess.push_back(edge.u_i);
			}
		}
	}
	let iter_2 = adj[s_i].iter();

	for edj in iter_2{
		let edge = edges[*edj].lock().unwrap();
		if edge.u_i == s_i {
			let v = nodes[edge.v_i].lock().unwrap();
			assert!(v.e == edge.c, "Edge: ({} -> {}) with flow: {}/{} \nNode: [{}], Excess: {}, Height: {}", edge.u_i, edge.v_i, edge.f, edge.c, v.i, v.e, v.h);
		}
		else {
			let u = nodes[edge.u_i].lock().unwrap();
			assert!(u.e == edge.c, "Edge: ({} -> {}) with flow: {}/{} \nNode: [{}], Excess: {}, Height: {}", edge.u_i, edge.v_i, edge.f, edge.c, u.i, u.e, u.h);	
		}
		assert!(edge.c == edge.f);
	}
	print_nodes(&nodes, LogLevel::Debug);
	print_edges(&edges, LogLevel::Debug);
	print_adj(&adj, LogLevel::Debug);

	debug!("rest of pushes");


	let mut handles: Vec<thread::JoinHandle<()>> = vec![];
	let n_threads: usize = 10;

	for i in 0..n_threads {
		let nodes_clone = nodes.clone();
		let edges_clone = edges.clone();
		let adj_clone = adj.clone();
		let excess_clone = Arc::clone(&excess);
		let handle  = thread::spawn(move || {
			push_or_relabel(&nodes_clone, &edges_clone, &excess_clone, &adj_clone, &s_i, &t_i, i);
		});
		handles.push(handle);
	}

	for handle in handles {
        handle.join().unwrap(); // Wait for all threads to finish
    }


	println!("{}", t_i);
	print_nodes(&nodes, LogLevel::Info);
	print_edges(&edges, LogLevel::Info);
	print_adj(&adj, LogLevel::Info);

	let sink_node = nodes[t_i].lock().unwrap();
	println!("f = {}", sink_node.e);


}
