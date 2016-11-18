#include "TessQuad.h"

#include <QtGlobal>

#include <QDebug>
#include <QFile>
#include <QImage>
#include <QTime>

#include <QVector2D>
#include <QVector3D>
#include <QMatrix4x4>

#include <cmath>
#include <cstring>

MyWindow::~MyWindow()
{
    if (mProgram != 0) delete mProgram;    
}

MyWindow::MyWindow()
    : mProgram(0), currentTimeMs(0), currentTimeS(0), angle(1.74533f), tPrev(0.0f), rotSpeed(M_PI / 8.0f)
{
    setSurfaceType(QWindow::OpenGLSurface);
    setFlags(Qt::Window | Qt::WindowSystemMenuHint | Qt::WindowTitleHint | Qt::WindowMinMaxButtonsHint | Qt::WindowCloseButtonHint);

    QSurfaceFormat format;
    format.setDepthBufferSize(24);
    format.setMajorVersion(4);
    format.setMinorVersion(3);
    format.setSamples(4);
    format.setProfile(QSurfaceFormat::CoreProfile);
    setFormat(format);
    create();

    resize(800, 600);

    mContext = new QOpenGLContext(this);
    mContext->setFormat(format);
    mContext->create();

    mContext->makeCurrent( this );

    mFuncs = mContext->versionFunctions<QOpenGLFunctions_4_3_Core>();
    if ( !mFuncs )
    {
        qWarning( "Could not obtain OpenGL versions object" );
        exit( 1 );
    }
    if (mFuncs->initializeOpenGLFunctions() == GL_FALSE)
    {
        qWarning( "Could not initialize core open GL functions" );
        exit( 1 );
    }

    initializeOpenGLFunctions();

    QTimer *repaintTimer = new QTimer(this);
    connect(repaintTimer, &QTimer::timeout, this, &MyWindow::render);
    repaintTimer->start(1000/60);

    QTimer *elapsedTimer = new QTimer(this);
    connect(elapsedTimer, &QTimer::timeout, this, &MyWindow::modCurTime);
    elapsedTimer->start(1);       
}

void MyWindow::modCurTime()
{
    currentTimeMs++;
    currentTimeS=currentTimeMs/1000.0f;
}

void MyWindow::initialize()
{
    mFuncs->glGenVertexArrays(1, &mVAO);
    mFuncs->glBindVertexArray(mVAO);    

    CreateVertexBuffer();
    initShaders();
    initMatrices();

    glFrontFace(GL_CCW);
    glEnable(GL_DEPTH_TEST);
}

void MyWindow::CreateVertexBuffer()
{    
    // Create and populate the buffer objects

    // Set up patch VBO
    //float v[] = {-1.0f, -1.0f, 1.0f, -1.0f, 1.0f, 1.0f, -1.0f, 1.0f};
    float v[] = {
                 // Top row
                 -0.9f,    0.3667f, -0.3667f, 0.3667f, -0.3667f, 0.9f, -0.9f,    0.9f,
                 -0.2667f, 0.3667f,  0.2667f, 0.3667f,  0.2667f, 0.9f, -0.2667f, 0.9f,
                  0.3667f, 0.3667f,  0.9f,    0.3667f,  0.9f,    0.9f,  0.3667f, 0.9f,

                 // Middle row
                 -0.9f,    -0.2667f, -0.3667f, -0.2667f, -0.3667f, 0.2667f, -0.9f,    0.2667f,
                 -0.2667f, -0.2667f,  0.2667f, -0.2667f,  0.2667f, 0.2667f, -0.2667f, 0.2667f,
                  0.3667f, -0.2667f,  0.9f,    -0.2667f,  0.9f,    0.2667f,  0.3667f, 0.2667f,

                 // Bottom row
                 -0.9f,    -0.9f, -0.3667f, -0.9f, -0.3667f, -0.3667f, -0.9f,    -0.3667f,
                 -0.2667f, -0.9f,  0.2667f, -0.9f,  0.2667f, -0.3667f, -0.2667f, -0.3667f,
                  0.3667f, -0.9f,  0.9f,    -0.9f,  0.9f,    -0.3667f,  0.3667f, -0.3667f};

    glGenBuffers(1, &mVBO);
    glBindBuffer(GL_ARRAY_BUFFER, mVBO);
    //glBufferData(GL_ARRAY_BUFFER, 72 * sizeof(float), v, GL_STATIC_DRAW);

    // Vertex positions
    mFuncs->glBindVertexBuffer(0, mVBO, 0, sizeof(GLfloat) * 2);
    mFuncs->glVertexAttribFormat(0, 2, GL_FLOAT, GL_FALSE, 0);
    mFuncs->glVertexAttribBinding(0, 0);

    mFuncs->glBindVertexArray(0);

    mFuncs->glPatchParameteri( GL_PATCH_VERTICES, 4);
}

void MyWindow::initMatrices()
{
    ViewMatrix.setToIdentity();
    ViewMatrix.lookAt(QVector3D(0.0f,0.0f,1.5f), QVector3D(0.0f, 0.0f, 0.0f), QVector3D(0.0f, 1.0f, 0.0f));
}

void MyWindow::resizeEvent(QResizeEvent *)
{
    mUpdateSize = true;

    ProjectionMatrix.setToIdentity();
    //ProjectionMatrix.perspective(70.0f, (float)this->width()/(float)this->height(), 0.3f, 100.0f);
    float c = 3.5f;
    ProjectionMatrix.ortho(-0.4f * c, 0.4f * c, -0.3f * c, 0.3f * c, 0.1f, 100.0f);

    float w2 = (float) this->width()  / 2.0f;
    float h2 = (float) this->height() / 2.0f;

    ViewPortMatrix = QMatrix4x4(w2,   0.0f, 0.0f, 0.0f,
                                0.0f, h2,   0.0f, 0.0f,
                                0.0f, 0.0f, 1.0f, 0.0f,
                                w2,   h2,   0.0f, 1.0f);

}

void MyWindow::render()
{

    float v[] = {
                 // Top row
                 -0.9f,    0.3667f, -0.3667f, 0.3667f, -0.3667f, 0.9f, -0.9f,    0.9f,
                 -0.2667f, 0.3667f,  0.2667f, 0.3667f,  0.2667f, 0.9f, -0.2667f, 0.9f,
                  0.3667f, 0.3667f,  0.9f,    0.3667f,  0.9f,    0.9f,  0.3667f, 0.9f,

                 // Middle row
                 -0.9f,    -0.2667f, -0.3667f, -0.2667f, -0.3667f, 0.2667f, -0.9f,    0.2667f,
                 -0.2667f, -0.2667f,  0.2667f, -0.2667f,  0.2667f, 0.2667f, -0.2667f, 0.2667f,
                  0.3667f, -0.2667f,  0.9f,    -0.2667f,  0.9f,    0.2667f,  0.3667f, 0.2667f,

                 // Bottom row
                 -0.9f,    -0.9f, -0.3667f, -0.9f, -0.3667f, -0.3667f, -0.9f,    -0.3667f,
                 -0.2667f, -0.9f,  0.2667f, -0.9f,  0.2667f, -0.3667f, -0.2667f, -0.3667f,
                  0.3667f, -0.9f,  0.9f,    -0.9f,  0.9f,    -0.3667f,  0.3667f, -0.3667f};

    if(!isVisible() || !isExposed())
        return;

    if (!mContext->makeCurrent(this))
        return;

    static bool initialized = false;
    if (!initialized) {
        initialize();
        initialized = true;
    }

    if (mUpdateSize) {
        glViewport(0, 0, size().width(), size().height());
        mUpdateSize = false;
    }

    float deltaT = currentTimeS - tPrev;
    if(tPrev == 0.0f) deltaT = 0.0f;
    tPrev = currentTimeS;
    angle += 0.25f * deltaT;
    if (angle > TwoPI) angle -= TwoPI;

    static float EvolvingVal = 0;
    EvolvingVal += 0.1f;

    //glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClearColor(0.5f,0.5f,0.5f,1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    //QMatrix4x4 RotationMatrix;
    //RotationMatrix.rotate(EvolvingVal, QVector3D(0.1f, 0.0f, 0.1f));
    //ModelMatrix.rotate(0.3f, QVector3D(0.1f, 0.0f, 0.1f));

    QMatrix4x4 mv1 = ViewMatrix * ModelMatrix;

    mFuncs->glBindVertexArray(mVAO);

    glEnableVertexAttribArray(0);

    mProgram->bind();
    {
        mProgram->setUniformValue("LineWidth", 1.5f);
        mProgram->setUniformValue("LineColor", QVector4D(0.05f,0.0f,0.05f,1.0f));
        mProgram->setUniformValue("QuadColor", QVector4D(1.0f,1.0f,1.0f,1.0f));

        mProgram->setUniformValue("MVP", ProjectionMatrix * mv1);
        mProgram->setUniformValue("ViewportMatrix", ViewPortMatrix);

        glBindBuffer(GL_ARRAY_BUFFER, mVBO);
        glBufferData(GL_ARRAY_BUFFER, 8 * sizeof(float), &v[0], GL_STATIC_DRAW);
        mProgram->setUniformValue("Inner", 2);
        mProgram->setUniformValue("Outer", 2);
        glDrawArrays(GL_PATCHES, 0, 4);

        glBufferData(GL_ARRAY_BUFFER, 8 * sizeof(float), &v[8], GL_STATIC_DRAW);
        mProgram->setUniformValue("Inner", 2);
        mProgram->setUniformValue("Outer", 4);
        glDrawArrays(GL_PATCHES, 0, 4);

        glBufferData(GL_ARRAY_BUFFER, 8 * sizeof(float), &v[16], GL_STATIC_DRAW);
        mProgram->setUniformValue("Inner", 2);
        mProgram->setUniformValue("Outer", 8);
        glDrawArrays(GL_PATCHES, 0, 4);

        glBufferData(GL_ARRAY_BUFFER, 8 * sizeof(float), &v[24], GL_STATIC_DRAW);
        mProgram->setUniformValue("Inner", 4);
        mProgram->setUniformValue("Outer", 2);
        glDrawArrays(GL_PATCHES, 0, 4);

        glBufferData(GL_ARRAY_BUFFER, 8 * sizeof(float), &v[32], GL_STATIC_DRAW);
        mProgram->setUniformValue("Inner", 4);
        mProgram->setUniformValue("Outer", 4);
        glDrawArrays(GL_PATCHES, 0, 4);

        glBufferData(GL_ARRAY_BUFFER, 8 * sizeof(float), &v[40], GL_STATIC_DRAW);
        mProgram->setUniformValue("Inner", 4);
        mProgram->setUniformValue("Outer", 8);
        glDrawArrays(GL_PATCHES, 0, 4);

        glBufferData(GL_ARRAY_BUFFER, 8 * sizeof(float), &v[48], GL_STATIC_DRAW);
        mProgram->setUniformValue("Inner", 8);
        mProgram->setUniformValue("Outer", 2);
        glDrawArrays(GL_PATCHES, 0, 4);

        glBufferData(GL_ARRAY_BUFFER, 8 * sizeof(float), &v[56], GL_STATIC_DRAW);
        mProgram->setUniformValue("Inner", 8);
        mProgram->setUniformValue("Outer", 4);
        glDrawArrays(GL_PATCHES, 0, 4);

        glBufferData(GL_ARRAY_BUFFER, 8 * sizeof(float), &v[64], GL_STATIC_DRAW);
        mProgram->setUniformValue("Inner", 8);
        mProgram->setUniformValue("Outer", 8);
        glDrawArrays(GL_PATCHES, 0, 4);
    }
    mProgram->release();

    glDisableVertexAttribArray(0);

    mContext->swapBuffers(this);
}

void MyWindow::initShaders()
{
    QOpenGLShader vShader(QOpenGLShader::Vertex);
    QOpenGLShader tcsShader(QOpenGLShader::TessellationControl);
    QOpenGLShader tesShader(QOpenGLShader::TessellationEvaluation);
    QOpenGLShader gShader(QOpenGLShader::Geometry);
    QOpenGLShader fShader(QOpenGLShader::Fragment);
    QFile         shaderFile;
    QByteArray    shaderSource;

    shaderFile.setFileName(":/vshader.txt");
    shaderFile.open(QIODevice::ReadOnly);
    shaderSource = shaderFile.readAll();
    shaderFile.close();
    qDebug() << "vertex    compile: " << vShader.compileSourceCode(shaderSource);

    shaderFile.setFileName(":/tcsshader.txt");
    shaderFile.open(QIODevice::ReadOnly);
    shaderSource = shaderFile.readAll();
    shaderFile.close();
    qDebug() << "tess ctrl compile: " << tcsShader.compileSourceCode(shaderSource);

    shaderFile.setFileName(":/tesshader.txt");
    shaderFile.open(QIODevice::ReadOnly);
    shaderSource = shaderFile.readAll();
    shaderFile.close();
    qDebug() << "tess eval compile: " << tesShader.compileSourceCode(shaderSource);

    shaderFile.setFileName(":/gshader.txt");
    shaderFile.open(QIODevice::ReadOnly);
    shaderSource = shaderFile.readAll();
    shaderFile.close();
    qDebug() << "geom      compile: " << gShader.compileSourceCode(shaderSource);

    shaderFile.setFileName(":/fshader.txt");
    shaderFile.open(QIODevice::ReadOnly);
    shaderSource = shaderFile.readAll();
    shaderFile.close();
    qDebug() << "frag      compile: " << fShader.compileSourceCode(shaderSource);

    mProgram = new (QOpenGLShaderProgram);
    mProgram->addShader(&vShader);
    mProgram->addShader(&tcsShader);
    mProgram->addShader(&tesShader);
    mProgram->addShader(&fShader);
    mProgram->addShader(&gShader);
    qDebug() << "shader link (mProgram): " << mProgram->link();
}

void MyWindow::PrepareTexture(GLenum TextureUnit, GLenum TextureTarget, const QString& FileName, bool flip)
{
    QImage TexImg;

    if (!TexImg.load(FileName)) qDebug() << "Erreur chargement texture " << FileName;
    if (flip==true) TexImg=TexImg.mirrored();

    glActiveTexture(TextureUnit);
    GLuint TexObject;
    glGenTextures(1, &TexObject);
    glBindTexture(TextureTarget, TexObject);
    mFuncs->glTexStorage2D(TextureTarget, 1, GL_RGBA8, TexImg.width(), TexImg.height());
    mFuncs->glTexSubImage2D(TextureTarget, 0, 0, 0, TexImg.width(), TexImg.height(), GL_BGRA, GL_UNSIGNED_BYTE, TexImg.bits());
    //glTexImage2D(TextureTarget, 0, GL_RGB, TexImg.width(), TexImg.height(), 0, GL_BGRA, GL_UNSIGNED_BYTE, TexImg.bits());
    glTexParameteri(TextureTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(TextureTarget, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

void MyWindow::keyPressEvent(QKeyEvent *keyEvent)
{
    switch(keyEvent->key())
    {
        case Qt::Key_P:
            break;
        case Qt::Key_Up:
            break;
        case Qt::Key_Down:
            break;
        case Qt::Key_Left:
            break;
        case Qt::Key_Right:
            break;
        case Qt::Key_Delete:
            break;
        case Qt::Key_PageDown:
            break;
        case Qt::Key_Home:
            break;
        case Qt::Key_Z:
            break;
        case Qt::Key_Q:
            break;
        case Qt::Key_S:
            break;
        case Qt::Key_D:
            break;
        case Qt::Key_A:
            break;
        case Qt::Key_E:
            break;
        default:
            break;
    }
}

void MyWindow::printMatrix(const QMatrix4x4& mat)
{
    const float *locMat = mat.transposed().constData();

    for (int i=0; i<4; i++)
    {
        qDebug() << locMat[i*4] << " " << locMat[i*4+1] << " " << locMat[i*4+2] << " " << locMat[i*4+3];
    }
}
