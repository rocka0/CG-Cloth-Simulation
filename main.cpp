#include <GL/glut.h>
#include <windows.h>
#include <stdio.h>

void init()
{
    glClearColor(1.0, 1.0, 1.0, 0.0);
    glColor3f(0.0f, 0.0f, 0.0f);
    glPointSize(1.0);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0.0, 600.0, 0.0, 600.0);
}

void setPixel(GLint xcoordinate, GLint ycoordinate)
{
    glBegin(GL_POINTS);
    glVertex2i(xcoordinate, ycoordinate);
    glEnd();
    glFlush();
}

void swap(GLint& a, GLint& b) {
    GLint temp = a;
    a = b;
    b = temp;
}

void midpointLine(GLint x0, GLint y0, GLint x1, GLint y1)
{
    GLint dx = x1 - x0;
    GLint dy = y1 - y0;
    GLint incrE, incrNE, x, y, d, iny;
    int fl = 0;
    if (abs(dx) < abs(dy)) { //to check if abs(m)>1)
        swap(x0, y0);
        swap(x1, y1);
        swap(dx, dy);
        fl = 1;
    }

    if (dx < 0) { // if x0>x1 start drawing from x1 to x0
        swap(x0, x1);
        swap(y0, y1);
        dx = -dx;
        dy = -dy;
    }

    if (dy >= 0) {
        iny = 1;
    }
    else {
        dy = -dy;
        iny = -1;
    }
    d = 2 * dy - dx;
    incrE = 2 * dy;
    incrNE = 2 * (dy - dx);
    x = x0, y = y0;

    (fl) ? setPixel(y, x) : setPixel(x, y);
    while (x < x1) {
        if (d <= 0) {
            d += incrE;
        }
        else {
            d += incrNE;
            y += iny;
        }
        x++;
        (fl) ? setPixel(y, x) : setPixel(x, y);
    }
}

void Draw_circle(GLint x, GLint y, GLint ox, GLint oy) {
    setPixel(x + ox, y + oy);
    setPixel(-x + ox, y + oy);
    setPixel(x + ox, -y + oy);
    setPixel(-x + ox, -y + oy);
    setPixel(y + ox, x + oy);
    setPixel(-y + ox, x + oy);
    setPixel(y + ox, -x + oy);
    setPixel(-y + ox, -x + oy);
}

void midpointCircle(GLint ox, GLint oy, GLint r) {
    GLint x = 0;
    GLint y = r;
    GLint d = 1 - r;
    GLint deltaE = 3;
    GLint deltaSE = -2 * r + 5;
    Draw_circle(x, y, ox, oy);
    while (y > x) {
        if (d < 0) {
            d += deltaE;
            deltaE += 2;
            deltaSE += 2;
        }
        else {
            d += deltaSE;
            deltaE += 2;
            deltaSE += 4;
            y--;
        }
        x++;
        Draw_circle(x, y, ox, oy);
    }
}

void ellipsePoints(GLint x, GLint y, GLint ox, GLint oy) {
    setPixel(x + ox, y + oy);
    setPixel(-x + ox, y + oy);
    setPixel(x + ox, -y + oy);
    setPixel(-x + ox, -y + oy);
}

void midpointEllipse(GLint ox, GLint oy, GLint a, GLint b) {
    double d2;
    GLint x = 0;
    GLint y = b;
    double d1 = (b * b) - (a * a * b) + (0.25 * a * a);
    ellipsePoints(x, y, ox, oy);

    while ((a * a) * (y - 0.5) > (b * b) * (x + 1)) {
        if (d1 < 0) {
            d1 += b * b * (2 * x + 3);
        }
        else {
            d1 += b * b * (2 * x + 3) + a * a * (-2 * y + 2);
            y--;
        }
        x++;
        ellipsePoints(x, y, ox, oy);
    }

    d2 = ((b * b) * (x + 0.5) * (x + 0.5)) + (a * a * (y - 1) * (y - 1)) - (a * a * b * b);
    while (y > 0) {
        if (d2 < 0) {
            d2 += (b * b * (2 * x + 2)) + ((a * a) * (-2 * y + 3));
            x++;
        }
        else {
            d2 += ((a * a) * (-2 * y + 3));
        }
        y--;
        ellipsePoints(x, y, ox, oy);
    }
}

void rect(GLint x0, GLint y0, GLint x1, GLint y1) {
    midpointLine(x0, y0, x1, y0);
    midpointLine(x1, y0, x1, y1);
    midpointLine(x1, y1, x0, y1);
    midpointLine(x0, y1, x0, y0);
}

void clock(GLint ox, GLint oy, GLint r) {
    //glClear(GL_COLOR_BUFFER_BIT);
    midpointCircle(ox, oy, r);
    midpointLine(ox - r / 2, oy, ox, oy);
    midpointLine(ox, oy + 3 * r / 4, ox, oy);
}

void school(void) {
    glClear(GL_COLOR_BUFFER_BIT);
    rect(100, 100, 410, 250);
    clock(255, 300, 50);
    for (int i = 110; i < 250; i += 50) {
        for (int j = 120; j < 400; j += 50) {
            rect(j, i, j + 20, i + 20);
        }
    }
}

void pacifier(void) {
    glClear(GL_COLOR_BUFFER_BIT);
    midpointCircle(100, 190, 30);
    midpointCircle(100, 100, 40);
    midpointCircle(100, 100, 20);
    midpointLine(50, 100, 150, 100);
    midpointLine(50, 110, 150, 110);
    midpointLine(50, 100, 50, 110);
    midpointLine(150, 100, 150, 110);
    midpointEllipse(60, 140, 30, 40);
    midpointEllipse(140, 140, 30, 40);
}

void Display(void)
{
    //pacifier();
    school();
}

int main(int argc, char** argv)
{
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_SINGLE | GLUT_RGB);
    glutInitWindowSize(600, 600);
    glutInitWindowPosition(0, 0);
    glutCreateWindow("Computer graphics");
    glutDisplayFunc(Display);
    init();
    glutMainLoop();
    return 0;
}