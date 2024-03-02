#include <iostream>

class Polygon {
	protected:
		int width, height;
	public:
		void set_values (int a, int b) {
			width = a;
			height = b;
		}
		virtual int area() {
			return 0;
		}
};

class Rectangle : public Polygon {
	public:
		int area() {
			return width * height;
		}
};

class Triangle : public Polygon {
	public:
		int area() {
			return (width * height) / 2;
		}
};

void regular_function() {
	printf("Hello from regular function\n");
}

int main(void) {

	Rectangle rect;
	Triangle trgl;
	volatile Polygon poly;

	Polygon *ppoly1 = &rect;
	Polygon *ppoly2 = &trgl;	

	ppoly1->set_values(66,77);
	ppoly2->set_values(89,90);

	ppoly1->area();
	ppoly2->area();
	
	regular_function();

	return 0;
}
