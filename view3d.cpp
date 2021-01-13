#include "view3d.h"


#include "QTimer"
#include "QFont"


View3d::View3d( QWidget *parent, int number, bool background/*, bool print*/) :
    QGLWidget( parent )
{
    ViewRadius = 3000;
    ViewAngleX = 0;
    ViewAngleY = 0;
    print = false;
    Number = number;
    Background = background;
    setAutoFillBackground(false);

 /*   Vertex = new GLfloat*[MAX_POINTS_PER_REV];//[MAX_LINES_COUNT];
    for (int i = 0; i < MAX_POINTS_PER_REV; i++)
    {
        Vertex[i] = new GLfloat[MAX_LINES_COUNT];
    }

    V = new Vector*[MAX_LINES_COUNT];//[MAX_POINTS_PER_REV];
    for (int i = 0; i < MAX_LINES_COUNT; i++)
    {
        V[i] = new Vector[MAX_POINTS_PER_REV];
    }


    InV = new Vector*[2];//[MAX_POINTS_PER_REV];
    for (int i = 0; i < 2; i++)
    {
        InV[i] = new Vector[MAX_POINTS_PER_REV];
    }*/



}

View3d::~View3d()
{

  /*  for (int i = 0; i < MAX_POINTS_PER_REV; i++)
    {
        delete [] Vertex[i];
    }
    delete [] Vertex;


    for (int i = 0; i < MAX_LINES_COUNT; i++)
    {
        delete [] V[i];
    }
    delete [] V;


    for (int i = 0; i < 2; i++)
    {
        delete InV[i];
    }
    delete [] InV;*/
}

//---------------------------------------------------------------------------------------------
void View3d::initializeGL()
{
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_NORMALIZE);

    if (Background)
        glClearColor(0.75, 0.75, 1.0, 0.0);
    else glClearColor(1.0, 1.0, 1.0, 0.0);
    //glShadeModel(GL_FLAT); // отключаю сглаживание краев

    glEnable(GL_CULL_FACE);           // устанавливается режим, когда строятся только внешние поверхности

    SetupLighting();
}

//---------------------------------------------------------------------------------------------
void View3d::resizeGL(int nWidth, int nHeight)
{
    w =nWidth;
    h = nHeight;
    if (h == 0)  // Предотвращение деления на ноль, если окно слишком мало
        h = 1;
    aspect = w / h;

    glViewport(0, 0, w, h);  //Сброс текущей области вывода и перспективных преобразований
    //glMatrixMode(GL_PROJECTION);
    //glLoadIdentity();



    // считаю ограничение по вертикали, т.к. PerspectiveAngle задается для высоты окна
//    double distance = 0.9 * h/2./3.7/atan(PerspectiveAngle/2.*M_PI/180.); // расстояние от точки наблюдения до окна наблюдения
//                                                                    // 3.7 - количество пикселей в 1 мм
//                                                                    // умножаю h на 0.9, чтобы остались поля
    UpdateViewRadius();

}

//---------------------------------------------------------------------------------------------
void View3d::paintGL()
{
    Angle = atan((BilletBottomDiameter - BilletTopDiameter)/2./BilletHeight);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION);  // Выбор матрицы проекции
    glLoadIdentity(); //Сброс матрицы проекции

    if (Mode == MODE_3D || Mode == VIEW_RESULT_3D)
        gluPerspective(PerspectiveAngle, aspect, 0.05, 150000.0 );
    else
    {
        // масштабирование области просмотра
        // ширину окна всегда устанавливаю равной 2.0
        // высоту устанавливаю пропорционально ширине
        windowHeight = 1.0f / aspect;
        glOrtho( -1.0, 1.0, -windowHeight, windowHeight, 1.0, -1.0 );

    }
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    if (Mode == MODE_3D || Mode == VIEW_RESULT_3D)
    {
        glTranslated(0, 0, -ViewRadius);
        glRotated(90, -1.0, .0, 0.0);
        glRotated(20, 1.0, .0, 0.0);

        glRotated(ViewAngleX, 1.0, 0.0, 0.0);
        glRotated(ViewAngleY, 0.0, .0, 1.0);

        glPushMatrix();
        {
            glTranslated(0, 0, -BilletHeight/2.);
            glRotated(-CurrentAngle-45, .0, .0, 1.0);
            glDisable(GL_LIGHTING);
            glDisable(GL_LIGHT0);
            DrawColorRing();
        }
        glPopMatrix();

        // рисую ПЭП
        if (Mode == MODE_3D)
        {
            glPushMatrix();
            // смещаюсь на нижний край заготовки (сначала в центр основания)
            glTranslatef(0, 0, -BilletHeight/2.);

            // поворачиваю систему координат на 45 градусов - так будет расположен ПЭП, чтобы его было хорошо видно
            glRotatef(45, 0,0,-1);

            // теперь смещаюсь на край изделия
            glTranslatef(BilletBottomDiameter/2., 0, 0);
            // направляю ось вдоль образующей изделия
            glRotatef(-Angle/M_PI*180., 0,1,0);
            // смещаюсь немного от поверхности изделия, чтоб датчик не сливался с ним
            // и вверх по образующей в соответствии с текущей координатой
            glTranslatef(5, 0, ScannerFormingCoord);

            // теперь еще раз поворачиваю систему координат, просто чтобы удобно было рисовать
            // параллелепипеды, символизирующие ПЭП, друг за другом вдоль оси Z
            glRotatef(90, 0,1,0);

            glEnable(GL_LIGHTING);
            glEnable(GL_LIGHT0);
            GLfloat MaterialAmbient[] =           { 1, 1, 1, 0.9 };
            GLfloat MaterialDiffuse[] =           { 1, 1, 1, 0.9 };
            GLfloat MaterialSpecular[] =          { 0.2, 0.2, 0.2, 0.0 };
            GLfloat MaterialShininess[] = {100.0};

            glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, MaterialAmbient);
            glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, MaterialDiffuse);
            glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, MaterialSpecular);
            glMaterialfv(GL_FRONT_AND_BACK, GL_SHININESS, MaterialShininess);

            DrawBox(35,35, 10);

            glTranslatef(00,0,10);
            GLfloat MaterialAmbient2[] =            { 0.7, 0.7, 0.6, 0.9 };
            GLfloat MaterialDiffuse2[] =            { 0.7, 0.7, 0.6, 0.9 };
            GLfloat MaterialSpecular2[] =           { 0.35, 0.35, 0.3, 0.0 };
            GLfloat MaterialShininess2[] = {100.0};

            glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, MaterialAmbient2);
            glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, MaterialDiffuse2);
            glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, MaterialSpecular2);
            glMaterialfv(GL_FRONT_AND_BACK, GL_SHININESS, MaterialShininess2);

            DrawBox(32,32, 23);

            glPopMatrix();
        }

    }
    else if (Mode == MODE_2D || Mode == VIEW_RESULT_2D)
    {
        //glDepthMask(GL_FALSE);
        //glDisable(GL_DEPTH_TEST);
        glDisable(GL_LIGHTING);
        glDisable(GL_LIGHT0);

        // масштабирую изображение таким образом, чтобы оно размещалось в окне

        int max_size;
        if (BilletBottomDiameter > BilletTopDiameter) max_size = BilletBottomDiameter * M_PI;
        else max_size = BilletTopDiameter * M_PI;
        double ratio;

        if (BilletHeight/(h - 40) > max_size/(w - 60))
            ratio = 2./aspect/(double)BilletHeight*(h-40)/h;
        else
            ratio = 2./(double)max_size*(w-60)/w;
        if (print)
        {
            QImage image(100,100, QImage::Format_ARGB32); // создаю QImage просто для того, чтобы узнать экранное разрешение
            double relation = print_scale / (image.physicalDpiX() / 25.4);
            if (BilletHeight/(2500 - 40*relation) > max_size/(2500 - 60*relation))
                ratio = 2./aspect/(double)BilletHeight*(2500-40*relation)/2500.;
            else
                ratio = 2./(double)max_size*(2500-60*relation)/2500.;
        }

        DrawCscan(ratio);
    }
}


//---------------------------------------------------------------------------
void DrawBox(double width, double depth, double height)
{
    Vector a, b, c, d, e, f, g, h;

    a.x = b.x = e.x = f.x = - width/2. ;
    c.x = d.x = g.x = h.x = width/2. ;
    a.y = e.y = h.y = d.y = depth/2.;
    f.y = g.y = b.y = c.y = - depth/2.;
    a.z = b.z = c.z = d.z = 0;
    e.z = f.z = g.z = h.z = height;

    glBegin(GL_QUADS);

     glNormal3d(0, 0, -1);
     glVertex3d(a.x,a.y,a.z);
     glVertex3d(d.x,d.y,d.z);
     glVertex3d(c.x,c.y,c.z);
     glVertex3d(b.x,b.y,b.z);

     glNormal3d(0, 1, 0);
     glVertex3d(a.x,a.y,a.z);
     glVertex3d(e.x,e.y,e.z);
     glVertex3d(h.x,h.y,h.z);
     glVertex3d(d.x,d.y,d.z);

     glNormal3d(1, 0, 0);
     glVertex3d(c.x,c.y,c.z);
     glVertex3d(d.x,d.y,d.z);
     glVertex3d(h.x,h.y,h.z);
     glVertex3d(g.x,g.y,g.z);

     glNormal3d(0, 0, 1);
     glVertex3d(e.x,e.y,e.z);
     glVertex3d(f.x,f.y,f.z);
     glVertex3d(g.x,g.y,g.z);
     glVertex3d(h.x,h.y,h.z);

     glNormal3d(0, -1, 0);
     glVertex3d(c.x,c.y,c.z);
     glVertex3d(g.x,g.y,g.z);
     glVertex3d(f.x,f.y,f.z);
     glVertex3d(b.x,b.y,b.z);

     glNormal3d(-1, 0, 0);
     glVertex3d(a.x,a.y,a.z);
     glVertex3d(b.x,b.y,b.z);
     glVertex3d(f.x,f.y,f.z);
     glVertex3d(e.x,e.y,e.z);

    glEnd();
}

//-------------------------------------------------------------------------------
void View3d::mousePressEvent(QMouseEvent *event)
{
    pressPosition = event->pos();
}
//-------------------------------------------------------------------------------
void View3d::mouseMoveEvent(QMouseEvent *event)
{
    ViewAngleX += (180 * ((GLfloat)event->y() - (GLfloat)pressPosition.y())) / h;
    ViewAngleY += (180 * ((GLfloat)event->x() - (GLfloat)pressPosition.x())) / w;
    pressPosition = event->pos();

    updateGL();
}
//-------------------------------------------------------------------------------
void View3d::wheelEvent(QWheelEvent *event)
{
    if (event->delta()>0) ViewRadius *= 1.1;
    else ViewRadius /= 1.1;
    updateGL();
}
//-------------------------------------------------------------------------------
void View3d::mouseDoubleClickEvent(QMouseEvent *event)
{
    emit zoom_c_scan(Number, Mode);
}
//-------------------------------------------------------------------------------

void View3d::SetupLighting()
{
    GLfloat AmbientLightPosition[] = {10, 10,0, -5};
    GLfloat LightAmbient[] = {0.5, 0.5, 0.5, 1.0};

    glLightfv(GL_LIGHT0, GL_POSITION, AmbientLightPosition);
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, LightAmbient);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    //glShadeModel(GL_SMOOTH);
}
//---------------------------------------------------------------------------

void View3d::DrawColorRing()
{      // строю усеченный конус вдоль оси z
    // буду строить сверху вниз и против часовой стрелки, чтобы наносить С-скан свеху вниз и слева направо
    // 13.01.2020 Перестраиваю снизу вверх, так как вроде бы контроль будет вестись снизу вверх
    // если требуется, рисую сетку координат
    double R = BilletBottomDiameter/2;
    double r = BilletTopDiameter/2;
    double length = BilletHeight;

    if (DrawScale)
    {
        glBegin(GL_LINES);
        glColor3d(0., 0.0, 0);
        // угловая сетка - через каждые 30 градусов
        for (int i = 0; i < 12; i++)
        {
            glVertex3d((r+1)*cos(i*2*M_PI/12.), (r+1)*sin(i*2*M_PI/12.), length  );
            glVertex3d((R+1)*cos(i*2*M_PI/12.), (R+1)*sin(i*2*M_PI/12.), 0);
        }
        glEnd();

        // сетка высот
        int scan_height = 0;
        GLdouble prev_x;
        GLdouble prev_y;

        while (scan_height <= length)
        {
            glBegin(GL_LINES);
            for (int i=0; i<=50; i++)
            {
                GLdouble x = (r-(r-R)*(length-scan_height)/length+2)*cos(i*2*M_PI/50);
                GLdouble y = (r-(r-R)*(length-scan_height)/length+2)*sin(i*2*M_PI/50);
                GLdouble z = scan_height;
                if (i) {glVertex3d(prev_x, prev_y, z);
                    glVertex3d(x, y,z);}
                prev_x = x;
                prev_y = y;
            }
            glEnd();
            scan_height += 100 ;
        }


        // вывожу надписи с координатами
        // внутри glBegin() - glEnd() это сделать не получается - весь скан становится черным

        double radius = R;
        GLfloat z =  -30;

        if (r > R)
        {
            radius = r;
            z = length + 30;
        }

        for (int i = 0; i < 12; i++)
        {
            if (sin(i*2*M_PI/12.+(ViewAngleY-CurrentAngle-45)*M_PI/180.)<0)
                renderText((radius+30)*cos(i*2*M_PI/12.), (radius+30)*sin(i*2*M_PI/12.), z, QString::number(i*30));
        }
        glPushMatrix();
        //glRotatef();
        glRotated(-ViewAngleY+CurrentAngle+45, 0.0, .0, 1.0);

        scan_height = 0;
        while (scan_height <= length)
        {
            GLdouble x = (r-(r-R)*(length-scan_height)/length+30);
            GLdouble y = 0;
            GLdouble z = scan_height;

            renderText(x, y, z, QString::number(scan_height));
            scan_height += 100 ;
        }
        glPopMatrix();
    }

    // выделяю дефекты, если требуется
    if (HighlightFlaws)
    {
        glColor3d(1, 1, 1);
        float x, y, z;
        for (int i = 0; i < FlawCount[Number]; i++)
        {
            glBegin(GL_LINE_STRIP);
                  x = (r-(r-R)*(length-DefFormStart[Number][i]*cos(Angle))/length)*cos(2*M_PI*DegreeCircStart[Number][i]/360./100);
                  y = (r-(r-R)*(length-DefFormStart[Number][i]*cos(Angle))/length)*sin(2*M_PI*DegreeCircStart[Number][i]/360./100);
                  z = (DefFormStart[Number][i])*cos(Angle);
                  glVertex3d(x, y, z);

                  x = (r-(r-R)*(length-DefFormStart[Number][i]*cos(Angle))/length)*cos(2*M_PI*DegreeCircEnd[Number][i]/360./100);
                  y = (r-(r-R)*(length-DefFormStart[Number][i]*cos(Angle))/length)*sin(2*M_PI*DegreeCircEnd[Number][i]/360./100);
                  z = (DefFormStart[Number][i])*cos(Angle);
                  glVertex3d(x, y, z);

                  x = (r-(r-R)*(length-DefFormEnd[Number][i]*cos(Angle))/length)*cos(2*M_PI*DegreeCircEnd[Number][i]/360./100);
                  y = (r-(r-R)*(length-DefFormEnd[Number][i]*cos(Angle))/length)*sin(2*M_PI*DegreeCircEnd[Number][i]/360./100);
                  z = (DefFormEnd[Number][i])*cos(Angle);
                  glVertex3d(x, y, z);

                  x = (r-(r-R)*(length-DefFormEnd[Number][i]*cos(Angle))/length)*cos(2*M_PI*DegreeCircStart[Number][i]/360./100);
                  y = (r-(r-R)*(length-DefFormEnd[Number][i]*cos(Angle))/length)*sin(2*M_PI*DegreeCircStart[Number][i]/360./100);
                  z = (DefFormEnd[Number][i])*cos(Angle);
                  glVertex3d(x, y, z);

                  x = (r-(r-R)*(length-DefFormStart[Number][i]*cos(Angle))/length)*cos(2*M_PI*DegreeCircStart[Number][i]/360./100);
                  y = (r-(r-R)*(length-DefFormStart[Number][i]*cos(Angle))/length)*sin(2*M_PI*DegreeCircStart[Number][i]/360./100);
                  z = (DefFormStart[Number][i])*cos(Angle);
                  glVertex3d(x, y, z);
            glEnd();
            // подписи с номерами дефектов


            if (sin(2*M_PI*DegreeCircEnd[Number][i]/360./100+(ViewAngleY-CurrentAngle-45)*M_PI/180.)<0)
            {
                x = (r-(r-R)*(length-DefFormEnd[Number][i]*cos(Angle))/length)*cos(2*M_PI*DegreeCircEnd[Number][i]/360./100);
                y = (r-(r-R)*(length-DefFormEnd[Number][i]*cos(Angle))/length)*sin(2*M_PI*DegreeCircEnd[Number][i]/360./100);
                z = (DefFormEnd[Number][i])*cos(Angle);
                renderText(x, y, z, QString::number(i+1));
            }
        }
    }

    //вычисление вершин
    // буду разбивать поверхность изделия на "пояса", высота которых равна шагу контроля


    // буду рисовать без освещения, поэтому нормали не считаю
    //вычисление нормалей к граням


    // строю боковую поверхность

    glBegin(GL_QUADS);
    for (int j=0; j < DrawTotalLinesCount; j++)
    {

        for (int i=0; i < DrawStepPerRevolution; i++)
        {
            if (j >= DrawStartLine && j < DrawStartLine + DrawLinesCount)
            {
                // устанавливаю цвет
                int a = PressedCScan[Number][j - DrawStartLine][i];
                if ((a != -1) /*|| Mode == VIEW_RESULT_3D*/)
                    glColor3d(a/128., 0.0, (128-a)/128.);
                else glColor3d(0.8, .8, .8);
            }
            else glColor3d(0.8, .8, .8);


            float x, y, z;

           /* x = (r-(r-R)*(DrawTotalLinesCount-j)/DrawTotalLinesCount)*cos(i*2*M_PI/DrawStepPerRevolution);
            y = (r-(r-R)*(DrawTotalLinesCount-j)/DrawTotalLinesCount)*sin(i*2*M_PI/DrawStepPerRevolution);
            z = j*length/DrawTotalLinesCount;
            glVertex3d(x, y, z);

            x = (r-(r-R)*(DrawTotalLinesCount-j)/DrawTotalLinesCount)*cos((i+1)*2*M_PI/DrawStepPerRevolution);
            y = (r-(r-R)*(DrawTotalLinesCount-j)/DrawTotalLinesCount)*sin((i+1)*2*M_PI/DrawStepPerRevolution);
            glVertex3d(x, y, z);

            x = (r-(r-R)*(DrawTotalLinesCount-j-1)/DrawTotalLinesCount)*cos((i+1)*2*M_PI/DrawStepPerRevolution);
            y = (r-(r-R)*(DrawTotalLinesCount-j-1)/DrawTotalLinesCount)*sin((i+1)*2*M_PI/DrawStepPerRevolution);
            z = (j+1)*length/DrawTotalLinesCount;
            glVertex3d(x, y, z);

            x = (r-(r-R)*(DrawTotalLinesCount-j-1)/DrawTotalLinesCount)*cos(i*2*M_PI/DrawStepPerRevolution);
            y = (r-(r-R)*(DrawTotalLinesCount-j-1)/DrawTotalLinesCount)*sin(i*2*M_PI/DrawStepPerRevolution);
            glVertex3d(x, y, z);*/

            glVertex3d(SideVertex[j][i].x,       SideVertex[j][i].y,      SideVertex[j][i].z);
            glVertex3d(SideVertex[j][i+1].x,     SideVertex[j][i+1].y,    SideVertex[j][i+1].z);
            glVertex3d(SideVertex[j+1][i+1].x,   SideVertex[j+1][i+1].y,  SideVertex[j+1][i+1].z);
            glVertex3d(SideVertex[j+1][i].x,     SideVertex[j+1][i].y,    SideVertex[j+1][i].z);


        }

    }
    // строю внутреннюю поверхность
    glColor3d(0.5, 0.5, 0.5);
    //glBegin(GL_QUADS);
    float x, y, z;
    for (int i=0;i<DrawStepPerRevolution;i++)
    {

       /* x = (r-20)*cos(i*2*M_PI/DrawStepPerRevolution);
        y = (r-20)*sin(i*2*M_PI/DrawStepPerRevolution);
        z = length;
        glVertex3d(x, y, z);

        x = (r-20)*cos((i+1)*2*M_PI/DrawStepPerRevolution);
        y = (r-20)*sin((i+1)*2*M_PI/DrawStepPerRevolution);
        glVertex3d(x, y, z);

        x = (R-20)*cos((i+1)*2*M_PI/DrawStepPerRevolution);
        y = (R-20)*sin((i+1)*2*M_PI/DrawStepPerRevolution);
        z = 0;
        glVertex3d(x, y, z);

        x = (R-20)*cos(i*2*M_PI/DrawStepPerRevolution);
        y = (R-20)*sin(i*2*M_PI/DrawStepPerRevolution);
        glVertex3d(x, y, z);*/
        glVertex3d(InV[0][i].x,InV[0][i].y,InV[0][i].z);
        glVertex3d(InV[0][i+1].x,InV[0][i+1].y,InV[0][i+1].z);

        glVertex3d(InV[1][i+1].x,InV[1][i+1].y,InV[1][i+1].z);
        glVertex3d(InV[1][i].x,InV[1][i].y,InV[1][i].z);


    }

    // строю дно (нижняя часть)
    glColor3d(0.7, 0.7, 0.7);

    z = 0;
    for (int i = 0; i<DrawStepPerRevolution;i++)
    {
       /* x = (r-(r-R)*(DrawTotalLinesCount)/DrawTotalLinesCount)*cos(i*2*M_PI/DrawStepPerRevolution);
        y = (r-(r-R)*(DrawTotalLinesCount)/DrawTotalLinesCount)*sin(i*2*M_PI/DrawStepPerRevolution);
        glVertex3d(x, y, z);

        x = (R-20)*cos(i*2*M_PI/DrawStepPerRevolution);
        y = (R-20)*sin(i*2*M_PI/DrawStepPerRevolution);
        glVertex3d(x, y, z);

        x = (R-20)*cos((i+1)*2*M_PI/DrawStepPerRevolution);
        y = (R-20)*sin((i+1)*2*M_PI/DrawStepPerRevolution);
        glVertex3d(x, y, z);

        x = (r-(r-R)*(DrawTotalLinesCount)/DrawTotalLinesCount)*cos((i+1)*2*M_PI/DrawStepPerRevolution);
        y = (r-(r-R)*(DrawTotalLinesCount)/DrawTotalLinesCount)*sin((i+1)*2*M_PI/DrawStepPerRevolution);
        glVertex3d(x, y, z);*/

        glVertex3d(SideVertex[0][i].x, SideVertex[0][i].y, SideVertex[0][i].z);
        glVertex3d(InV[1][i].x,InV[1][i].y,InV[1][i].z);
        glVertex3d(InV[1][i+1].x,InV[1][i+1].y,InV[1][i+1].z);
        glVertex3d(SideVertex[0][i+1].x, SideVertex[0][i+1].y, SideVertex[0][i+1].z);

    }

    // второе дно,
    z = length;
    for (int i = 0; i<DrawStepPerRevolution; i++)
    {
       /* x = r*cos(i*2*M_PI/DrawStepPerRevolution);
        y = r*sin(i*2*M_PI/DrawStepPerRevolution);
        glVertex3d(x, y, z);

        x = r*cos((i+1)*2*M_PI/DrawStepPerRevolution);
        y = r*sin((i+1)*2*M_PI/DrawStepPerRevolution);
        glVertex3d(x, y, z);

        x = (r-20)*cos((i+1)*2*M_PI/DrawStepPerRevolution);
        y = (r-20)*sin((i+1)*2*M_PI/DrawStepPerRevolution);
        glVertex3d(x, y, z);

        x = (r-20)*cos(i*2*M_PI/DrawStepPerRevolution);
        y = (r-20)*sin(i*2*M_PI/DrawStepPerRevolution);
        glVertex3d(x, y, z);*/

        glVertex3d(SideVertex[DrawTotalLinesCount][i].x,   SideVertex[DrawTotalLinesCount][i].y,    SideVertex[DrawTotalLinesCount][i].z);
        glVertex3d(SideVertex[DrawTotalLinesCount][i+1].x, SideVertex[DrawTotalLinesCount][i+1].y,  SideVertex[DrawTotalLinesCount][i+1].z);
        glVertex3d(InV[0][i+1].x,InV[0][i+1].y,InV[0][i+1].z);
        glVertex3d(InV[0][i].x,InV[0][i].y,InV[0][i].z);


    }

    glEnd();
}



//-----------------------------------------------------------------------
void View3d::DrawCscan(double rate)
{
//    glColor3f(0,0,0);
//
    double R = BilletBottomDiameter * M_PI;
    double r = BilletTopDiameter * M_PI;
    double length = BilletHeight;
    glDepthMask(GL_FALSE);
    // если требуется, рисую сетку координат

    //вычисление вершин
    // вершины боковой поверхности
    // M - по горизонтали
    // N - по вертикали
    // вычисляю только горизонтальные координаты


    /*QOpenGLTexture texture(TextureImage->mirrored());

    texture.bind();
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_ALPHA_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);*/


    glBegin(GL_QUADS);
    for (int j=0; j<DrawTotalLinesCount; j++)
    {
        for (int i=0; i<DrawStepPerRevolution; i++)
        {
            if (j >= DrawStartLine && j < DrawStartLine + DrawLinesCount)
            {
                // устанавливаю цвет
                int a = PressedCScan[Number][j - DrawStartLine][i];
                if ((a != -1) /*|| Mode == VIEW_RESULT_2D*/)
                glColor3d(a/128., 0.0, (128-a)/128.);
                else glColor3d(0.7, .7, .7);
            }
            else glColor3d(0.7, .7, .7);

            // 30.01.2020 проверил - расчет верный
            // смысл такой: развертку рисую в виде трапеции
            // вычисляю координату левой границы трапеции на высоте, соответствующей текущей линии ("поясу")
            // и прибавляю к этому значению такую часть длины текущей горизонтальной линии трапеции, которая соответствует тому,
            // какую часть составляет текущая координата по окружности от полной окружности
           /* float x = (R-r)*(j)/2./DrawTotalLinesCount + (R - (R-r)*(j)/(float)DrawTotalLinesCount)*i/DrawStepPerRevolution - R/2.;
            glVertex2d(x*rate, (-length/2 + (j)*length/DrawTotalLinesCount)*rate);

            x = (R-r)*(j)/2./DrawTotalLinesCount + (R - (R-r)*(j)/(float)DrawTotalLinesCount)*(i+1)/DrawStepPerRevolution - R/2.;
            glVertex2d(x*rate, (-length/2 + (j)*length/DrawTotalLinesCount)*rate);

            x = (R-r)*(j+1)/2./DrawTotalLinesCount + (R - (R-r)*(j+1)/(float)DrawTotalLinesCount)*(i+1)/DrawStepPerRevolution - R/2.;
            glVertex2d(x*rate, (-length/2 + (j+1)*length/DrawTotalLinesCount)*rate);

            x = (R-r)*(j+1)/2./DrawTotalLinesCount + (R - (R-r)*(j+1)/(float)DrawTotalLinesCount)*(i)/DrawStepPerRevolution - R/2.;
            glVertex2d(x*rate, (-length/2 + (j+1)*length/DrawTotalLinesCount)*rate);*/


            glVertex2d(Vertex[i][j]*rate, (-length/2 + (j)*length/DrawTotalLinesCount)*rate);
            glVertex2d(Vertex[i+1][j]*rate, (-length/2 + (j)*length/DrawTotalLinesCount)*rate);
            glVertex2d(Vertex[i+1][j+1]*rate, (-length/2 + (j+1)*length/DrawTotalLinesCount)*rate);
            glVertex2d(Vertex[i][j+1]*rate, (-length/2 + (j+1)*length/DrawTotalLinesCount)*rate);



        }
    }
    glEnd();
//glColor3d(0.7, .7, .7);
  /*  glBegin(GL_TRIANGLES);//_STRIP);

    glTexCoord2f(1, 1);
    glVertex2d(r/2.*rate, (length/2.)*rate);

    glTexCoord2f(0.0, 0);
     glVertex2d(- R/2.*rate, (-length/2.)*rate);

     glTexCoord2f(1, 0.);
     glVertex2d(R/2.*rate, (-length/2.)*rate);


     glTexCoord2f(0.0, 0);
     glVertex2d(- R/2.*rate, (-length/2.)*rate);

     glTexCoord2f(1, 1);
     glVertex2d(r/2.*rate, (length/2.)*rate);

     glTexCoord2f(0.0, 1);
     glVertex2d(-r/2.*rate, (length/2.)*rate);


    glEnd();
    glDisable(GL_TEXTURE_2D);*/

    if (DrawScale)
    {
        glBegin(GL_LINES);
        glColor3d(0., 0.0, 0);
        // угловая сетка - через каждые 30 градусов
        for (int i = 0; i <= 12; i++)
        {
            glVertex2d(-R/2.*rate + i*R/12.*rate, -length/2.*rate);
            glVertex2d(-r/2.*rate + i*r/12.*rate, length/2.*rate);
        }

        // сетка высот
        int scan_height = 0;
        while (scan_height <= length)
        {
            double x_begin = -R/2. + (R-r)/2. * scan_height/length;
            double x_end = -x_begin;

            glVertex2d(x_begin*rate, (scan_height-length/2.)*rate);
            glVertex2d(x_end*rate, (scan_height-length/2.)*rate);

            scan_height += 100 ;
        }

        glEnd();

        // вывожу надписи с координатами
        // внутри glBegin() - glEnd() это сделать не получается - весь скан становится черным
        //glColor3d(0., 0.0, 0);
        double radius = R;
        GLfloat y = (-length/2.)*rate;
        if (R >= r)
        {
            for (int i = 0; i <= 12; i++)
                PaintText(-radius/2.*rate + i*radius/12.*rate, y, QString::number(i*30), 8, Qt::black);
        }
        else
        {
            radius = r;
            y = (length/2.)*rate;
            for (int i = 0; i <= 12; i++)
                PaintText(-radius/2.*rate + i*radius/12.*rate, y, QString::number(i*30), 8, Qt::black, false, above);
        }


        scan_height = 0;

        while (scan_height <= length)
        {
            double x = -R/2. + (R-r)/2. * scan_height/length;

            PaintText(x*rate, (-length/2. + scan_height)*rate, QString::number(scan_height), 8, Qt::black, false, rear);
            scan_height += 100 ;
        }
    }

    // выделяю дефекты, если требуется
    if (HighlightFlaws)
    {
        glColor3d(1, 1, 1);

        for (int i = 0; i < FlawCount[Number]; i++)
        {
            float x;
            if (DegreeCircStart[Number][i] <= DegreeCircEnd[Number][i])
            {

            glBegin(GL_LINE_STRIP);
                  x = (R-r)*(DefFormStart[Number][i]*cos(Angle))/2./length + (R-(R-r)*(DefFormStart[Number][i]*cos(Angle))/length)*DegreeCircStart[Number][i]/360./100 - R/2.;
                  glVertex2d(x*rate, (-length/2 + DefFormStart[Number][i]*cos(Angle))*rate);
                  x = (R-r)*(DefFormStart[Number][i]*cos(Angle))/2./length + (R-(R-r)*(DefFormStart[Number][i]*cos(Angle))/length)*DegreeCircEnd[Number][i]/360./100 - R/2.;
                  glVertex2d(x*rate, (-length/2 + DefFormStart[Number][i]*cos(Angle))*rate);
                  x = (R-r)*(DefFormEnd[Number][i]*cos(Angle))/2./length + (R-(R-r)*(DefFormEnd[Number][i]*cos(Angle))/length)*DegreeCircEnd[Number][i]/360./100 - R/2.;
                  glVertex2d(x*rate, (-length/2 + DefFormEnd[Number][i]*cos(Angle))*rate);
                  x = (R-r)*(DefFormEnd[Number][i]*cos(Angle))/2./length + (R-(R-r)*(DefFormEnd[Number][i]*cos(Angle))/length)*DegreeCircStart[Number][i]/360./100 - R/2.;
                  glVertex2d(x*rate, (-length/2 + DefFormEnd[Number][i]*cos(Angle))*rate);
                  x = (R-r)*(DefFormStart[Number][i]*cos(Angle))/2./length + (R-(R-r)*(DefFormStart[Number][i]*cos(Angle))/length)*DegreeCircStart[Number][i]/360./100 - R/2.;
                  glVertex2d(x*rate, (-length/2 + DefFormStart[Number][i]*cos(Angle))*rate);
            glEnd();
            }
            else // если дефект лежит на линии разрыва скана, то есть на левой и правой границах скана
            {
                glBegin(GL_LINE_STRIP);
                      x = (R-r)*(DefFormStart[Number][i]*cos(Angle))/2./length - R/2.;
                      glVertex2d(x*rate, (-length/2 + DefFormStart[Number][i]*cos(Angle))*rate);
                      x = (R-r)*(DefFormStart[Number][i]*cos(Angle))/2./length + (R-(R-r)*(DefFormStart[Number][i]*cos(Angle))/length)*DegreeCircEnd[Number][i]/360./100 - R/2.;
                      glVertex2d(x*rate, (-length/2 + DefFormStart[Number][i]*cos(Angle))*rate);
                      x = (R-r)*(DefFormEnd[Number][i]*cos(Angle))/2./length + (R-(R-r)*(DefFormEnd[Number][i]*cos(Angle))/length)*DegreeCircEnd[Number][i]/360./100 - R/2.;
                      glVertex2d(x*rate, (-length/2 + DefFormEnd[Number][i]*cos(Angle))*rate);
                      x = (R-r)*(DefFormEnd[Number][i]*cos(Angle))/2./length - R/2.;
                      glVertex2d(x*rate, (-length/2 + DefFormEnd[Number][i]*cos(Angle))*rate);


                glEnd();

                glBegin(GL_LINE_STRIP);
                      x = (R-r)*(DefFormStart[Number][i]*cos(Angle))/2./length - R/2.;
                      glVertex2d(x*rate, (-length/2 + DefFormStart[Number][i]*cos(Angle))*rate);
                      x = (R-r)*(DefFormStart[Number][i]*cos(Angle))/2./length + (R-(R-r)*(DefFormStart[Number][i]*cos(Angle))/length)*DegreeCircStart[Number][i]/360./100 - R/2.;
                      glVertex2d(x*rate, (-length/2 + DefFormStart[Number][i]*cos(Angle))*rate);
                      x = (R-r)*(DefFormEnd[Number][i]*cos(Angle))/2./length + (R-(R-r)*(DefFormEnd[Number][i]*cos(Angle))/length)*DegreeCircStart[Number][i]/360./100 - R/2.;
                      glVertex2d(x*rate, (-length/2 + DefFormEnd[Number][i]*cos(Angle))*rate);
                      x = (R-r)*(DefFormEnd[Number][i]*cos(Angle))/2./length - R/2.;
                      glVertex2d(x*rate, (-length/2 + DefFormEnd[Number][i]*cos(Angle))*rate);
                glEnd();
            }
            // подписи с номерами дефектов
          // renderText(Vertex[i][1]*rate, (-length/2. + DefXEnd[Number][i]*length/BilletHeight)*rate,1, QString::number(i+1));
            x = (R-r)*(DefFormEnd[Number][i]*cos(Angle))/2./length + (R-(R-r)*(DefFormEnd[Number][i]*cos(Angle))/length)*DegreeCircEnd[Number][i]/360./100 - R/2.;
            PaintText(x*rate, (-length/2. + DefFormEnd[Number][i])*rate, QString::number(i+1), 8, Qt::white, true, above);
        }
    }

    glDepthMask(GL_TRUE);
}
//------------------------------------------------------------------------------------------------------------------------------------
void View3d::PaintText(GLfloat x, GLfloat y, /*GLfloat z,*/ QString text, int font_size, QColor font_color, bool bold, int placement)
{
    QImage image(64, 32, QImage::Format_ARGB32);

    QPainter painter(&image);

    painter.begin(&image);
    if (print)
    {
        font_size *= print_scale / (image.physicalDpiX() / 25.4);
    }
    if (bold)
        painter.setFont(QFont("Arial", font_size, QFont::Bold));
    else
        painter.setFont(QFont("Arial", font_size, QFont::Normal));

    QFontMetrics fm = painter.fontMetrics();
    int h1 = fm.height();
    int w1 = fm.width(text);
    painter.end();

    // попробую подогнать размеры QImage под размеры текста
    QImage image2(w1+5, h1+5, QImage::Format_ARGB32);
    image2.fill(Qt::transparent);
    QPainter painter2(&image2);

    painter2.begin(&image);
    if (bold)
        painter2.setFont(QFont("Arial", font_size, QFont::Bold));
    else
        painter2.setFont(QFont("Arial", font_size, QFont::Normal));
    painter2.setPen(font_color);
    painter2.drawText(0, 0, w1, h1, Qt::AlignCenter, text);
    painter2.end();

    QOpenGLTexture texture(image2.mirrored());

    texture.bind();
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_ALPHA_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // рассчитываю размер прямоугольной области с надписью,
    // исходя из размеров окна
    GLfloat wdth, hght;

    if (print)
    {
        wdth = image2.width()*2/2500.;
        hght = image2.height()*2/2500.;
    }
    else
    {
        wdth = image2.width()*2/(double)width(); // ширина окна всегда соответствует 2.0
        hght = image2.height()*2/(double)height() / aspect;
    }


    glPushMatrix();

    glTranslatef(x, y, 0);
    //glDepthMask(GL_FALSE);
    //glDisable(GL_DEPTH_TEST);
    glBegin(GL_QUADS);
    if (placement == above)
    {
        glTexCoord2f(0.0, 0);
        glVertex2d(0, 0);

        glTexCoord2f(1, 0.);
        glVertex2d(wdth,0);

        glTexCoord2f(1.0f, 1.0f);
        glVertex2d(wdth, hght);

        glTexCoord2f(0.0, 1.0);
        glVertex2d(0, hght);
    }
    else if (placement == rear)
    {
        glTexCoord2f(0.0, 0);
        glVertex2d(-wdth, -hght/2.);

        glTexCoord2f(1, 0.);
        glVertex2d(0,-hght/2.);

        glTexCoord2f(1.0f, 1.0f);
        glVertex2d(0, hght/2.);

        glTexCoord2f(0.0, 1.0);
        glVertex2d(-wdth, hght/2.);
    }

    else
    {
        glTexCoord2f(0.0, 0);
        glVertex2d(0, -hght);

        glTexCoord2f(1, 0.);
        glVertex2d(wdth,-hght);

        glTexCoord2f(1.0f, 1.0f);
        glVertex2d(wdth, 0);

        glTexCoord2f(0.0, 1.0);
        glVertex2d(0, 0);
    }

    glEnd();
    glPopMatrix();
    glDisable(GL_TEXTURE_2D);
//glDepthMask(GL_TRUE);

}
//------------------------------------------------------------------------------------------------------------------
void View3d::UpdateViewRadius()
{
    float max_size = BilletHeight;
    ViewRadius = 1.4*max_size/2./tan(PerspectiveAngle/2.*M_PI/180.);
    //ViewRadius = max_size/tan(PerspectiveAngle/2.*M_PI/180.);

    // теперь посмотрю, хватит ли ширины окна
    //double hor_angle = atan(aspect*tan(PerspectiveAngle/2.*M_PI/180.)); // половина угла обзора окна в горизонтальной плоскости
    max_size = BilletBottomDiameter;
    if (BilletTopDiameter > max_size) max_size = BilletTopDiameter;
    double view_radius = max_size/2./0.9/(aspect*tan(PerspectiveAngle/2.*M_PI/180.)) ;
    if (view_radius > ViewRadius) ViewRadius = view_radius;
}
