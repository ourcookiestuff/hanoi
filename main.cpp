#include <osg/Group>
#include <osg/Geode>
#include <osg/ShapeDrawable>
#include <osg/PositionAttitudeTransform>
#include <osgViewer/Viewer>
#include <osgViewer/ViewerEventHandlers>
#include <osgGA/TrackballManipulator>
#include <osgGA/GUIEventHandler>
#include <osgUtil/LineSegmentIntersector>
#include <osgText/Text>
#include <osg/Camera>
#include <iostream>
#include <vector>
#include <osg/NodeCallback>
#include <osg/Timer>

// Struktura dysku
struct Disk {
    int size;
    osg::ref_ptr<osg::PositionAttitudeTransform> transform;    // do przesuwania dysku
    osg::ref_ptr<osg::Geode> geode;    // geometria dysku 
    std::string colorName;
};

// Struktura słupka
struct Pole {
    osg::ref_ptr<osg::Node> node;   // węzeł reprezentujący słupek
    std::vector<Disk*> disks;   // lista dysków na słupku
};

// automatycznie zamknięcie wygranej
class ExitAfterDelay : public osg::NodeCallback {
    double startTime;
    osgViewer::Viewer* viewer;
public:
    ExitAfterDelay(osgViewer::Viewer* v) : viewer(v), startTime(osg::Timer::instance()->time_s()) {}

    virtual void operator()(osg::Node* node, osg::NodeVisitor* nv) {
        double now = osg::Timer::instance()->time_s();
        if(now - startTime >= 3.0) {  // po 3 sekundach
            viewer->setDone(true);     // zamknij aplikację
            return;
        }
        traverse(node, nv);
    }
};

// Klasa zarządzająca grą
class HanoiGame : public osg::Referenced {
public:
    Pole poles[3];    // tablica trzech słupków
    int selectedPole;   // który słupek wybrano
    osg::ref_ptr<osgText::Text> hudText;    // tekst na ekranie
    int moveCount;    // liczba wykonanych ruchów

    // wskaźniki do głównej sceny i okna
    osg::Group* root;
    osgViewer::Viewer* viewer;
    
    HanoiGame() : selectedPole(-1), moveCount(0), root(nullptr), viewer(nullptr) {}

    // Gdzie powinien być dysk w przestrzeni 
    osg::Vec3 getDiskPosition(int poleIndex, int diskHeight) {
        float xPos[] = {-5.0f, 0.0f, 5.0f};    // pozycje X słupków
        return osg::Vec3(xPos[poleIndex], 0, 0.5f + diskHeight * 0.6f);
    }

    // czy ruch jest dozwolony
    bool isValidMove(int from, int to) {
        // if (from < 0 || from > 2 || to < 0 || to > 2) return false;
        if (poles[from].disks.empty()) return false; // nie można brać z pustego
        if (from == to) return false; // nie na ten sam
        if (poles[to].disks.empty()) return true; // jeśli docelowy jest pusty to ok

        int fromSize = poles[from].disks.back()->size;
        int toSize = poles[to].disks.back()->size;
        return fromSize < toSize; // ruch ok jeśli przenosimy mniejszy dysk na większy
    }

    // ruch dla dysku
    void moveDisk(int from, int to) {
        if (!isValidMove(from, to)) {
            setText("Nieprawidlowy ruch!");
            return;
        }

        moveCount++;

        Disk* disk = poles[from].disks.back(); // weź dysk ze słupka
        poles[from].disks.pop_back(); // usuń go ze słupka
        poles[to].disks.push_back(disk); // dodaj na docelowy słupek

        int height = poles[to].disks.size() - 1;
        disk->transform->setPosition(getDiskPosition(to, height)); // przesuń dysk w scenie 3D na nową pozycję

        setText("Ruch: " + std::to_string(moveCount) + " \nPrzeniesiono dysk " + disk->colorName +
                   " na slupek " + std::to_string(to + 1));

        if (poles[2].disks.size() == 4) {
            setText("GRATULACJE! WYGRALES W " + std::to_string(moveCount) + " RUCHACH!");
            root->setUpdateCallback(new ExitAfterDelay(viewer)); // timer - zamknij po 3s

        }
    }

    // podnoszenie dysku o 0.5 jednostki
    void highlightTopDisk(int poleIndex) {
        if (!poles[poleIndex].disks.empty()) {
            Disk* disk = poles[poleIndex].disks.back();
            osg::Vec3 pos = disk->transform->getPosition();
            pos.z() += 0.5f;
            disk->transform->setPosition(pos);
            setText("Wybrano slupek " + std::to_string(poleIndex + 1));
        }
    }

    // powrót na normalną wysokość
    void unhighlightTopDisk(int poleIndex) {
        if (!poles[poleIndex].disks.empty()) {
            int height = poles[poleIndex].disks.size() - 1;
            Disk* disk = poles[poleIndex].disks.back();
            disk->transform->setPosition(getDiskPosition(poleIndex, height));
        }
    }

    // zmiana tekstu na scenie
    void setText(const std::string& str) {
        if(hudText) hudText->setText(str);
    }
};

// klasa manipulatora kamery
class OrbitManipulator : public osgGA::TrackballManipulator {
private:
    double _distance;   // odległość od centrum sceny
    double _angle;      // kąt obrotu wokół sceny (w radianach)
    double _elevation;  // kąt wysokości kamery (stały)
    
public:
    OrbitManipulator() : osgGA::TrackballManipulator() {
        _distance = 30.0;   // 30 jednostek od centrum
        _angle = -osg::PI_2;  // Początkowo patrzymy od przodu (od strony gracza)
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
        return false;  // używamy do gry
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

// Handler kliknięć
class ClickHandler : public osgGA::GUIEventHandler {
public:
    osg::ref_ptr<HanoiGame> game;   // wskaźnik do gry

    ClickHandler(HanoiGame* g) : game(g) {}

    // główna funkcja - przy każdym kliknięciu myszki
    virtual bool handle(const osgGA::GUIEventAdapter& ea, osgGA::GUIActionAdapter& aa) {

        // TYLKO prawy przycisk myszy i tylko RELEASE (puszczenie przycisku)
        if (ea.getEventType() != osgGA::GUIEventAdapter::RELEASE)
            return false;
            
        if (ea.getButton() != osgGA::GUIEventAdapter::RIGHT_MOUSE_BUTTON)
            return false;

        // konwersja wskażnika na Viewer - żeby zobaczyć co klinkeliśmy
        osgViewer::Viewer* viewer = dynamic_cast<osgViewer::Viewer*>(&aa);
        if (!viewer) return false;

        int clickedPole = pickPole(ea, viewer);
        
        if (clickedPole == -1) {
            //std::cout << "Nie kliknięto na żaden słupek" << std::endl;
            return true;
        }

        if (game->selectedPole == -1) {
            // Pierwszy klik - wybór źródła
            if (game->poles[clickedPole].disks.empty()) {
                //std::cout << "Ten słupek jest pusty!" << std::endl;
                return true;
            }
            
            game->selectedPole = clickedPole; // zapis wybranego słupka
            game->highlightTopDisk(clickedPole); // podnieś go do góry
        } else {
            // Drugi klik - wykonaj ruch
            int from = game->selectedPole;
            game->unhighlightTopDisk(from);
            game->moveDisk(from, clickedPole);
            game->selectedPole = -1;
        }

        return true; // ok - obsłużyliśmy zdarzenie
    }

    // funkcja sprawdzająca w co klikneliśmy
    int pickPole(const osgGA::GUIEventAdapter& ea, osgViewer::Viewer* viewer) {
        // "strzela promień" z kamery przez pozycję myszy
        osg::ref_ptr<osgUtil::LineSegmentIntersector> intersector =
            new osgUtil::LineSegmentIntersector(
                osgUtil::Intersector::WINDOW,
                ea.getX(), ea.getY()
            );

        // odwiedzacz który przechodzi przez scenę i sprawdza kolizje
        osgUtil::IntersectionVisitor iv(intersector.get());
        // sprawdź wszystkie obiekty w scenie
        viewer->getCamera()->accept(iv);

        // pudło
        if (!intersector->containsIntersections())
            return -1;

        // Sprawdź który słupek został kliknięty
        for (const auto& intersection : intersector->getIntersections()) {
            // ścieżka węzłów od kamery do trafionego obiektu, sprawdzamy każdy węzeł na tej ścieżce
            for (auto* node : intersection.nodePath) {
                // sprawdzamy każdy z 3 słupków
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

// kamera do wyświetlania tekstu 2D
osg::ref_ptr<osg::Camera> createHUDCamera(float width, float height) {
    osg::ref_ptr<osg::Camera> camera = new osg::Camera; // druga kamera
    camera->setReferenceFrame(osg::Transform::ABSOLUTE_RF); //nie zależy od głównej kamery - ma własny układ współrzędnych
    camera->setClearMask(GL_DEPTH_BUFFER_BIT);
    camera->setRenderOrder(osg::Camera::POST_RENDER);
    camera->setAllowEventFocus(false);
    camera->setProjectionMatrix(osg::Matrix::ortho2D(0, width, 0, height));
    camera->setViewMatrix(osg::Matrix::identity());
    return camera;
}

// tworzenie tekstu na ekranie
osg::ref_ptr<osgText::Text> createHUDText(float x, float y, const std::string& str, float size = 20.0f) {
    osg::ref_ptr<osgText::Text> text = new osgText::Text;
    text->setFont(osgText::readFontFile("fonts/arial.ttf"));
    text->setCharacterSize(size);
    text->setPosition(osg::Vec3(x, y, 0));
    text->setText(str);
    text->setColor(osg::Vec4(1,1,1,1));
    return text;
}

int main() {
    osgViewer::Viewer viewer; // główne okno aplikacji 3D
    
    osg::ref_ptr<osg::Group> root = new osg::Group(); // korzeń drzewa sceny
    osg::ref_ptr<HanoiGame> game = new HanoiGame(); // obiekt gry
    
    game->root = root.get();
    game->viewer = &viewer;

    root->addChild(createBase()); // dodawanie podstawy wieży
    
    // dodawanie słupków
    for (int i = 0; i < 3; i++) {
        osg::ref_ptr<osg::Node> pole = createPole(-5.0f + i * 5.0f);
        root->addChild(pole);
        game->poles[i].node = pole;
    }
    
    std::string colorNames[] = {"CZERWONY","ZIELONY","NIEBIESKI","ZOLTY"};
    osg::Vec4 colors[] = {
        osg::Vec4(1.0, 0.2, 0.2, 1.0),   // Czerwony (największy)
        osg::Vec4(0.2, 1.0, 0.2, 1.0),   // Zielony
        osg::Vec4(0.2, 0.2, 1.0, 1.0),   // Niebieski
        osg::Vec4(1.0, 1.0, 0.2, 1.0)    // Żółty (najmniejszy)
    };
    
    // Utwórz dyski (od największego do najmniejszego)
    for (int i = 0; i < 4; i++) {
        float radius = 2.5 - i*0.5;
        // Tworzymy Transform dla dysku (żeby móc go przesuwać)
        osg::ref_ptr<osg::PositionAttitudeTransform> transform = new osg::PositionAttitudeTransform();
        transform->setPosition(game->getDiskPosition(0, i));

        osg::ref_ptr<osg::Geode> geom = createDiskGeometry(radius, colors[i]);
        transform->addChild(geom);
        root->addChild(transform);

        Disk* disk = new Disk();
        disk->size = 4 - i; // najmniejszy =1, największy=4
        disk->transform = transform;
        disk->geode = geom;
        disk->colorName = colorNames[i];
        game->poles[0].disks.push_back(disk);
    }
    
    viewer.setSceneData(root.get());
    
    // własny manipulator kamery
    OrbitManipulator* manipulator = new OrbitManipulator();
    viewer.setCameraManipulator(manipulator);
    manipulator->setAllowThrow(false);
    
    // Dodaj handler kliknięć (prawy przycisk myszy)
    viewer.addEventHandler(new ClickHandler(game.get()));
    
    // Dodaj handler statystyk
    viewer.addEventHandler(new osgViewer::StatsHandler);

    // Handler do zamykania (ESC)
    // viewer.addEventHandler(new osgViewer::StatsHandler);
    // viewer.addEventHandler(new osgViewer::WindowSizeHandler);
    // viewer.addEventHandler(new osgViewer::ThreadingHandler);
    // viewer.addEventHandler(new osgViewer::HelpHandler);
    
    viewer.realize();
    
    auto traits = viewer.getCamera()->getGraphicsContext()->getTraits();
    float width = traits->width;
    float height = traits->height;

    // stwórz HUD
    osg::ref_ptr<osg::Camera> hudCamera = createHUDCamera(width, height);
    osg::ref_ptr<osg::Geode> hudGeode = new osg::Geode();
    osg::ref_ptr<osgText::Text> hudText = createHUDText(10, height - 30, "Witaj w Wiezach Hanoi! Przenies wszystkie krazki ze slupka 1 na slupek 3");
    hudGeode->addDrawable(hudText);
    hudCamera->addChild(hudGeode);
    root->addChild(hudCamera);

    // zapisz wskaźnik do tekstu w obiekcie gry
    game->hudText = hudText;

    // Ustaw początkową pozycję kamery
    manipulator->updateCamera();
    
    // główna pętla gry
    return viewer.run();
}