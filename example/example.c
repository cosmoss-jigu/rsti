#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>

void hello_func(int x){
	printf("Hello! %d\n", x );
}

int bye_func(){ 
	printf("Bye!\n");
	return 1;
}

void p_func(void (*x)(), int (*y)()){
	printf("Func_pointer!\n");
	x(2);
	y(2);
}

typedef struct {
	int dummy1;
	int (*fp)();
	const char dummy2;
} S;

struct node {
   int data;
   int key;
   struct node *next;
   struct node *prev;
};

struct node2 {
   int data;
   int key;
   struct node2 *next;
};

void bar(struct node2 *q) {
	//authPAC(q, struct node2*_scope);
	printf("%d\n", q->key);
}

void foo1(struct node **p) {
	//authPAC(p, struct node*_scope);
	//addPAC(p, struct node2*_scope);
	struct node2** pp = (struct node2**) p;
	bar((struct node2 *)(*p)); // source type -> target type
}

void foo2(void **p) {
	//authPAC(p, void*_scope);
	//addPAC(p, struct node2*_scope);
	bar((struct node2 *)(*p));
}

//void foo3(void *p) {
//	bar((struct node2 *)(*p));
//}

//void foo4(unsigned long p) {
//	bar((struct node2 *)(*((void *)p)));
//}


int main(void)
{
	// * 1 - T
	// * 2 - F
	// * 3 - I
	//
	// * Check variable use-def chain. If any uses are arguments to functions
	// * with a bitcast, resort to type 1.
	//
	
	struct node* ptr = (struct node*) malloc(sizeof(struct node));
	//addPAC(ptr, struct node*_scope);
	const struct node* ptr_const = (struct node*) malloc(sizeof(struct node));
	//addPAC(ptr_const, struct node*_scope);
	struct node2* ptr2 = (struct node2*) malloc(sizeof(struct node2));
	//addPAC(ptr2, struct node2*_scope);
	
	// * Check for bitcast 
	// * if no bitcast, do nothing
	// * if bitcast
	// * auth and resign with type
	
	// * Do nothing
	foo1(&ptr);

	//authPAC(ptr2, struct node2*_scope);
	//addPAC(ptr2, struct node*_scope);
	foo1(&ptr2);
	
	//authPAC(ptr, struct node*_scope);
	//addPAC(ptr, void*_scope);
	foo2(&ptr);

	//authPAC(ptr2, struct node2*_scope);
	//addPAC(ptr2, void*_scope);
	foo2(&ptr2);
//	foo3(&ptr);
//	foo3(&ptr2);
//	foo4(&ptr);
//	foo4(&ptr2);

	printf("Next\n");

	struct node* a = ptr->prev;
	// addPAC(a, struct node*_scope);
	
	struct node2* b = ptr->next;
	// addPAC(b, struct node2*_scope);

	void** c;

	if(bye_func() == 1){
		// authPAC(a, struct node*_scope);
		c = &a;
		// addPAC(c, void*_scope);
		// addPAC(a, struct node*_scope);
	}
	else{
		// authPAC(b, struct node2*_scope);
		c = &b;
		// addPAC(c, void*_scope);
		// addPAC(b, void*_scope);
	}

	S* str = (S*) malloc(sizeof(S));
	str->fp = bye_func;
	str->fp();

	// authPAC(c, void*_scope);
	struct node2* z = (struct node2*) *c;
	// addPAC(z, struct node2*_scope);
	// addPAC(c, struct node2*_scope);
	//
	// * Possible optimization: Do once before store.
	//printf("check for load/store %lx\n", &b->next);
	//printf("check for load/store %lx\n", &ptr_const);
	
	return 0;

}
