Display         -       the qt widget (QOpenGLWidget) that displays stuff, handle mouse move, asks all renderers to draw things
Renderer        -       a virtual class, GeometryRenderer and AxesRenderer are subclasses
GeometryRenderer-       manages rendering a database
AxesRenderer    -       manages rendering axes
Camera          -       a virtual class, input is mouse/keyboard events etc, outputs projection and modelview matrices. OrthographicCamera is a subclass
DisplayManager  -       similar to dm_wgl.c. Renderers use this. (need to fix AxesRenderer to utilize this rather than direct opengl)

Display puts everything together
