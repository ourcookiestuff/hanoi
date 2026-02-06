#include <osg/Group>
#include <osg/Geode>
#include <osg/ShapeDrawable>
#include <osg/PositionAttitudeTransform>
#include <osgViewer/Viewer>
#include <osgViewer/ViewerEventHandlers>
#include <osgGA/TrackballManipulator>
#include <osg/Math>

class OrbitManipulator : public osgGA::TrackballManipulator {
private:
    double _distance;
    double _angle;  // kąt obrotu wokół sceny (w radianach)
    double _elevation;  // kąt wysokości kamery (stały)
    
public:
    OrbitManipulator() : osgGA::TrackballManipulator() {
        _distance = 30.0;
        _angle = osg::PI;  // Początkowo patrzymy z boku
        _elevation = 0.5;  // Stała wysokość kamery
    }
    
    virtual bool handleMouseWheel(const osgGA::GUIEventAdapter& ea, osgGA::GUIActionAdapter& us) {
        return false;  // Zablokuj zoom
    }
    
    virtual bool performMovementLeftMouseButton(const double eventTimeDelta, const double dx, const double dy) {
        // Tylko obrót w poziomie (wokół osi Z)
        _angle -= dx * 0.5;  // Zmień kąt w zależności od ruchu myszy
        updateCamera();
        return true;
    }
    
    virtual bool performMovementMiddleMouseButton(const double eventTimeDelta, const double dx, const double dy) {
        return false;  // Zablokuj przesuwanie
    }
    
    virtual bool performMovementRightMouseButton(const double eventTimeDelta, const double dx, const double dy) {
        return false;
    }
    
    void updateCamera() {
        // Oblicz pozycję kamery na podstawie kąta
        double x = _distance * cos(_angle) * cos(_elevation);
        double y = _distance * sin(_angle) * cos(_elevation);
        double z = _distance * sin(_elevation);
        
        osg::Vec3d eye(x, y, z);
        osg::Vec3d center(0, 0, 2);  // Patrz na środek sceny
        osg::Vec3d up(0, 0, 1);      // Góra to zawsze Z
        
        setTransformation(eye, center, up);
    }
};

// Funkcja tworząca podstawę
osg::Node* createBase() {
    osg::Geode* geode = new osg::Geode();
    osg::Box* box = new osg::Box(osg::Vec3(0, 0, -0.5), 15, 5, 1);
    osg::ShapeDrawable* drawable = new osg::ShapeDrawable(box);
    drawable->setColor(osg::Vec4(0.6, 0.4, 0.2, 1.0));
    geode->addDrawable(drawable);
    return geode;
}

// Funkcja tworząca słupek
osg::Node* createPole(float x) {
    osg::PositionAttitudeTransform* transform = new osg::PositionAttitudeTransform();
    transform->setPosition(osg::Vec3(x, 0, 0));
    
    osg::Geode* geode = new osg::Geode();
    osg::Cylinder* cylinder = new osg::Cylinder(osg::Vec3(0, 0, 3), 0.3, 6);
    osg::ShapeDrawable* drawable = new osg::ShapeDrawable(cylinder);
    drawable->setColor(osg::Vec4(0.3, 0.3, 0.3, 1.0));
    geode->addDrawable(drawable);
    
    transform->addChild(geode);
    return transform;
}

// Funkcja tworząca dysk
osg::Node* createDisk(float x, float z, float radius, osg::Vec4 color) {
    osg::PositionAttitudeTransform* transform = new osg::PositionAttitudeTransform();
    transform->setPosition(osg::Vec3(x, 0, z));
    
    osg::Geode* geode = new osg::Geode();
    osg::Cylinder* cylinder = new osg::Cylinder(osg::Vec3(0, 0, 0), radius, 0.5);
    osg::ShapeDrawable* drawable = new osg::ShapeDrawable(cylinder);
    drawable->setColor(color);
    geode->addDrawable(drawable);
    
    transform->addChild(geode);
    return transform;
}

int main() {
    osg::Group* root = new osg::Group();
    
    root->addChild(createBase());
    
    root->addChild(createPole(-5.0));
    root->addChild(createPole(0.0));
    root->addChild(createPole(5.0));
    
    osg::Vec4 colors[] = {
        osg::Vec4(1, 0, 0, 1),
        osg::Vec4(0, 1, 0, 1),
        osg::Vec4(0, 0, 1, 1),
        osg::Vec4(1, 1, 0, 1)
    };
    
    for (int i = 0; i < 4; i++) {
        float radius = 2.5 - i * 0.5;
        float z = 0.5 + i * 0.6;
        root->addChild(createDisk(-5.0, z, radius, colors[i]));
    }
    
    osgViewer::Viewer viewer;
    viewer.setSceneData(root);
    
    OrbitManipulator* manipulator = new OrbitManipulator();
    viewer.setCameraManipulator(manipulator);

    viewer.setUpViewInWindow(100, 100, 800, 600);
    viewer.addEventHandler(new osgViewer::WindowSizeHandler);

    viewer.realize();
    manipulator->updateCamera();  // początkowa pozycja kamery
    
    return viewer.run();
}