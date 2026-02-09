#include <osg/Group>
#include <osg/Geode>
#include <osg/ShapeDrawable>
#include <osg/PositionAttitudeTransform>
#include <osgViewer/Viewer>
#include <osgViewer/ViewerEventHandlers>
#include <osgGA/TrackballManipulator>
#include <osgGA/GUIEventHandler>
#include <osgUtil/LineSegmentIntersector>
#include <osg/Math>
#include <iostream>
#include <vector>

// Struktura dysku
struct Disk {
    int size;
    osg::ref_ptr<osg::PositionAttitudeTransform> transform;
    osg::ref_ptr<osg::Geode> geode;
};

// Struktura słupka
struct Pole {
    osg::ref_ptr<osg::Node> node;
    std::vector<Disk*> disks;
};

// Klasa zarządzająca grą
class HanoiGame : public osg::Referenced {
public:
    Pole poles[3];
    int selectedPole;
    
    HanoiGame() : selectedPole(-1) {}
    
    osg::Vec3 getDiskPosition(int poleIndex, int diskHeight) {
        float xPos[] = {-5.0f, 0.0f, 5.0f};
        return osg::Vec3(xPos[poleIndex], 0, 0.5f + diskHeight * 0.6f);
    }
    
    bool isValidMove(int from, int to) {
        if (from < 0 || from > 2 || to < 0 || to > 2) return false;
        if (poles[from].disks.empty()) return false;
        if (from == to) return false;
        if (poles[to].disks.empty()) return true;
        
        int fromSize = poles[from].disks.back()->size;
        int toSize = poles[to].disks.back()->size;
        return fromSize < toSize;
    }
    
    void moveDisk(int from, int to) {
        if (!isValidMove(from, to)) {
            std::cout << "❌ Nieprawidłowy ruch!" << std::endl;
            return;
        }
        
        Disk* disk = poles[from].disks.back();
        poles[from].disks.pop_back();
        poles[to].disks.push_back(disk);
        
        int height = poles[to].disks.size() - 1;
        disk->transform->setPosition(getDiskPosition(to, height));
        
        std::cout << "✓ Przeniesiono dysk rozmiaru " << disk->size 
                  << " ze słupka " << from << " na słupek " << to << std::endl;
        
        if (poles[2].disks.size() == 4) {
            std::cout << "\n🎉🎉🎉 GRATULACJE! WYGRAŁEŚ! 🎉🎉🎉\n" << std::endl;
        }
    }
    
    void highlightTopDisk(int poleIndex) {
        if (poleIndex >= 0 && poleIndex < 3 && !poles[poleIndex].disks.empty()) {
            Disk* disk = poles[poleIndex].disks.back();
            osg::Vec3 pos = disk->transform->getPosition();
            pos.z() += 0.5f;
            disk->transform->setPosition(pos);
            std::cout << "🔵 Wybrano słupek " << poleIndex << std::endl;
        }
    }
    
    void unhighlightTopDisk(int poleIndex) {
        if (poleIndex >= 0 && poleIndex < 3 && !poles[poleIndex].disks.empty()) {
            int height = poles[poleIndex].disks.size() - 1;
            Disk* disk = poles[poleIndex].disks.back();
            disk->transform->setPosition(getDiskPosition(poleIndex, height));
        }
    }
};

// Twój oryginalny manipulator kamery
class OrbitManipulator : public osgGA::TrackballManipulator {
private:
    double _distance;
    double _angle;      // kąt obrotu wokół sceny (w radianach)
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

// Handler kliknięć - używa PRAWEGO przycisku myszy aby nie kolidować z kamerą
class ClickHandler : public osgGA::GUIEventHandler {
public:
    osg::ref_ptr<HanoiGame> game;

    ClickHandler(HanoiGame* g) : game(g) {}

    virtual bool handle(const osgGA::GUIEventAdapter& ea,
                        osgGA::GUIActionAdapter& aa) {

        // TYLKO prawy przycisk myszy i tylko RELEASE (puszczenie przycisku)
        if (ea.getEventType() != osgGA::GUIEventAdapter::RELEASE)
            return false;
            
        if (ea.getButton() != osgGA::GUIEventAdapter::RIGHT_MOUSE_BUTTON)
            return false;

        osgViewer::Viewer* viewer = dynamic_cast<osgViewer::Viewer*>(&aa);
        if (!viewer) return false;

        int clickedPole = pickPole(ea, viewer);
        
        if (clickedPole == -1) {
            std::cout << "❌ Nie kliknięto na żaden słupek" << std::endl;
            return true;
        }

        if (game->selectedPole == -1) {
            // Pierwszy klik - wybór źródła
            if (game->poles[clickedPole].disks.empty()) {
                std::cout << "❌ Ten słupek jest pusty!" << std::endl;
                return true;
            }
            
            game->selectedPole = clickedPole;
            game->highlightTopDisk(clickedPole);
        } else {
            // Drugi klik - wykonaj ruch
            int from = game->selectedPole;
            game->unhighlightTopDisk(from);
            game->moveDisk(from, clickedPole);
            game->selectedPole = -1;
        }

        return true;
    }

    int pickPole(const osgGA::GUIEventAdapter& ea, osgViewer::Viewer* viewer) {
        osg::ref_ptr<osgUtil::LineSegmentIntersector> intersector =
            new osgUtil::LineSegmentIntersector(
                osgUtil::Intersector::WINDOW,
                ea.getX(), ea.getY()
            );

        osgUtil::IntersectionVisitor iv(intersector.get());
        viewer->getCamera()->accept(iv);

        if (!intersector->containsIntersections())
            return -1;

        // Sprawdź który słupek został kliknięty
        for (const auto& intersection : intersector->getIntersections()) {
            for (auto* node : intersection.nodePath) {
                for (int i = 0; i < 3; i++) {
                    // Sprawdź czy kliknięto na słupek
                    if (node == game->poles[i].node.get()) {
                        return i;
                    }
                    
                    // Sprawdź czy kliknięto na dysk tego słupka
                    for (auto* disk : game->poles[i].disks) {
                        if (node == disk->transform.get() || node == disk->geode.get()) {
                            return i;
                        }
                    }
                }
            }
        }

        return -1;
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

// Funkcja tworząca geometrię dysku
osg::Geode* createDiskGeometry(float radius, osg::Vec4 color) {
    osg::Geode* geode = new osg::Geode();
    osg::Cylinder* cylinder = new osg::Cylinder(osg::Vec3(0, 0, 0), radius, 0.5);
    osg::ShapeDrawable* drawable = new osg::ShapeDrawable(cylinder);
    drawable->setColor(color);
    geode->addDrawable(drawable);
    return geode;
}

int main() {
    osg::ref_ptr<osg::Group> root = new osg::Group();
    osg::ref_ptr<HanoiGame> game = new HanoiGame();
    
    root->addChild(createBase());
    
    // Dodaj słupki
    for (int i = 0; i < 3; i++) {
        osg::ref_ptr<osg::Node> pole = createPole(-5.0f + i * 5.0f);
        root->addChild(pole);
        game->poles[i].node = pole;
    }
    
    osg::Vec4 colors[] = {
        osg::Vec4(1.0, 0.2, 0.2, 1.0),   // Czerwony (największy)
        osg::Vec4(0.2, 1.0, 0.2, 1.0),   // Zielony
        osg::Vec4(0.2, 0.2, 1.0, 1.0),   // Niebieski
        osg::Vec4(1.0, 1.0, 0.2, 1.0)    // Żółty (najmniejszy)
    };
    
    // Utwórz dyski (od największego do najmniejszego)
    for (int i = 0; i < 4; i++) { 
        float radius = 2.5 - i * 0.5;
        
        osg::ref_ptr<osg::PositionAttitudeTransform> transform = 
            new osg::PositionAttitudeTransform();
        transform->setPosition(game->getDiskPosition(0, i));
        
        osg::ref_ptr<osg::Geode> diskGeom = createDiskGeometry(radius, colors[i]);
        transform->addChild(diskGeom);
        root->addChild(transform);
        
        Disk* disk = new Disk();
        disk->size = i;  // 0 = największy, 3 = najmniejszy
        disk->transform = transform;
        disk->geode = diskGeom;
        
        game->poles[0].disks.push_back(disk);
    }
    
    osgViewer::Viewer viewer;
    viewer.setSceneData(root.get());
    
    // Użyj Twojego oryginalnego manipulatora kamery
    OrbitManipulator* manipulator = new OrbitManipulator();
    viewer.setCameraManipulator(manipulator);
    manipulator->setAllowThrow(false);
    
    // Dodaj handler kliknięć (prawy przycisk myszy)
    viewer.addEventHandler(new ClickHandler(game.get()));
    
    // Dodaj handler statystyk
    viewer.addEventHandler(new osgViewer::StatsHandler);
    
    viewer.realize();
    
    auto traits = viewer.getCamera()->getGraphicsContext()->getTraits();
    std::cout << "Rozmiar okna: " << traits->width << " x " << traits->height << std::endl;
    viewer.getCamera()->setViewport(0, 0, traits->width, traits->height);
    
    // Ustaw początkową pozycję kamery
    manipulator->updateCamera();
    
    std::cout << "\n╔═══════════════════════════════════════╗" << std::endl;
    std::cout << "║       🗼  WIEŻE HANOI  🗼            ║" << std::endl;
    std::cout << "╚═══════════════════════════════════════╝" << std::endl;
    std::cout << "\n📋 INSTRUKCJE:" << std::endl;
    std::cout << "  🖱️  LEWY przycisk + ruch myszy - obrót kamery wokół sceny" << std::endl;
    std::cout << "  🖱️  PRAWY przycisk myszy - graj:" << std::endl;
    std::cout << "      1️⃣  Kliknij na słupek źródłowy (dysk się podniesie)" << std::endl;
    std::cout << "      2️⃣  Kliknij na słupek docelowy (dysk się przeniesie)" << std::endl;
    std::cout << "  🎯 CEL: Przenieś wszystkie dyski na prawy słupek!" << std::endl;
    std::cout << "  📏 ZASADA: Większy dysk nie może leżeć na mniejszym\n" << std::endl;
    
    return viewer.run();
}