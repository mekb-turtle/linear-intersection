#include <stdio.h>
#include <stdlib.h>
#include <err.h>
#include <errno.h>
#include <getopt.h>
#include <stdbool.h>
#include <math.h>
#include <float.h>
#include <signal.h>

static struct option options_getopt[] = {
        {"help",       no_argument, 0, 'h'},
        {"subtract90", no_argument, 0, 's'},
        {"degrees",    no_argument, 0, 'd'},
        {"radians",    no_argument, 0, 'r'},
        {0,            0,           0, 0  }
};

void parse_double_argument(char *argv[], char *arg_name, int *arg_index, double *value) {
	char *string = argv[*arg_index];
	char *endptr = string;
	double val = strtod(string, &endptr);
	int class = fpclassify(val);
	if (endptr == string || *endptr != '\0' || (class != FP_NORMAL && class != FP_ZERO)) {
		errx(2, "%s: %s: not a valid floating point number", arg_name, string);
	}
	*value = val;
	++*arg_index;
}

#define DIGITS (DBL_DECIMAL_DIG - 1)
#define FLOAT "%.*g"
#define FLOAT_POS "%+.*g"

enum mode {
	GRADIENT,
	DEGREES,
	RADIANS
} mode = GRADIENT;
bool subtract90 = false;

void print_line(double x, double y, double m, double m_actual) {
	printf("y=");
	if (m_actual != 0) {
		if (m_actual != 1) {
			if (mode != GRADIENT) printf("tan(");
			printf(FLOAT, DIGITS, m);
			if (mode == DEGREES) printf("°");
			if (subtract90) {
				if (mode == RADIANS) printf("-\u03c0/2"); // pi symbol
				if (mode == DEGREES) printf("-90°");
			}
			if (mode != GRADIENT) printf(")");
			printf("*");
		}
		if (x != 0)
			printf("(x" FLOAT_POS ")", DIGITS, -x);
		else
			printf("x");
		if (y != 0) printf(FLOAT_POS, DIGITS, y);
	} else
		printf(FLOAT, DIGITS, y);
	printf("\n");
}

int main(int argc, char *argv[]) {
	bool invalid = false;
	int opt;

	// argument handling
	while ((opt = getopt_long(argc, argv, ":hsdr", options_getopt, NULL)) != -1) {
		switch (opt) {
			case 'h':
				printf("Usage: %s [OPTION]... x1 y1 m1 x2 y2 m2\n", PROJECT_NAME);
				printf("put -- before the arguments if using negative numbers\n");
				printf("y = m1*(x-x1) + y1, where x1 and y1 are the offsets\n");
				printf("if you want to use regular y = m*x + c, set x1 to 0 and y1 as c\n");
				printf("m1 is the gradient\n");
				printf("same goes for x2/y2/m2\n");
				printf("-h --help: Shows help text\n");
				printf("-s --subtract90: Subtract 90° from the gradient (useful for Minecraft stronghold)\n");
				printf("-d --degrees: Use degrees for the gradient\n");
				printf("-r --radians: Use radians for the gradient\n");
				return 0;
			default:
				if (!invalid) {
					switch (opt) {
						case 's':
							subtract90 = true;
							break;
						case 'd':
							if (mode != GRADIENT) invalid = true;
							else
								mode = DEGREES;
							break;
						case 'r':
							if (mode != GRADIENT) invalid = true;
							else
								mode = RADIANS;
							break;
						default:
							invalid = true;
							break;
					}
				}
				break;
		}
	}

	if (optind != argc - 6 || invalid)
		errx(1, "Invalid usage, try --help");

	if (mode == GRADIENT && subtract90) {
		errx(1, "--subtract90 requires --radians or --degrees");
	}

	errno = 0;

	// parse arguments as floating point values
	struct line {
		double x, y, gradient_input, gradient;
	} lines[2];

#define x1 (lines[0].x)
#define y1 (lines[0].y)
#define m1 (lines[0].gradient)
#define m1_ (lines[0].gradient_input)
#define x2 (lines[1].x)
#define y2 (lines[1].y)
#define m2 (lines[1].gradient)
#define m2_ (lines[1].gradient_input)

	int i = optind;
	parse_double_argument(argv, "x1", &i, &x1);
	parse_double_argument(argv, "y1", &i, &y1);
	parse_double_argument(argv, "m1", &i, &m1_);
	parse_double_argument(argv, "x2", &i, &x2);
	parse_double_argument(argv, "y2", &i, &y2);
	parse_double_argument(argv, "m2", &i, &m2_);

	m1 = m1_;
	m2 = m2_;

	// find gradient
	switch (mode) {
		case DEGREES:
			m1 = m1 * M_PI / 180.0;
			m2 = m2 * M_PI / 180.0;
		case RADIANS:
			if (subtract90) {
				m1 -= M_PI_2;
				m2 -= M_PI_2;
			}
			m1 = tan(m1);
			m2 = tan(m2);
			break;
		default:
			break;
	}

	// point-slope form linear equation
	// y = m1*(x-x1) + y1
	// make two equations equal each other and solve for x
	// m1*(x-x1) + y1 = m2*(x-x2) + y2
	// m1*(x-x1) - m2*(x-x2) + y1 = y2
	// m1*(x-x1) - m2*(x-x2) = y2 - y1
	// m1*x - m1*x1 - m2*x + m2*x2 = y2 - y1
	// m1*x - m2*x - m1*x1 + m2*x2 = y2 - y1
	// x*(m1-m2) - m1*x1 + m2*x2 = y2 - y1
	// x*(m1-m2) - m1*x1 = y2 - y1 - m2*x2
	// x*(m1-m2) = y2 - y1 - m2*x2 + m1*x1
	// x = (y2 - y1 - m2*x2 + m1*x1) / (m1-m2)
	// implies m1 != m2 because two parallel lines never intersect

	double gradient = m1 - m2;
	int class = fpclassify(gradient);
	switch (class) {
		case FP_NORMAL:
			break;
		case FP_ZERO:
			errx(3, "Lines cannot be parallel");
		default:
			errx(3, "Error calculating intersection");
	}

	double x_intersect = (y2 - y1 - m2 * x2 + m1 * x1) / (gradient);
	double y_intersect = m1 * (x_intersect - x1) + y1; // substitute value in

	print_line(x1, y1, m1_, m1);
	print_line(x2, y2, m2_, m2);
	printf("x = " FLOAT "\n", DIGITS, x_intersect);
	printf("y = " FLOAT "\n", DIGITS, y_intersect);
	printf("(" FLOAT ", " FLOAT ")\n", DIGITS, x_intersect, DIGITS, y_intersect);

	return 0;
}
