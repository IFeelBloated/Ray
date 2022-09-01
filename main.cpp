#include <QCoreApplication>
#include <QCommandLineParser>
#include <QImage>
#include <QtCore>

#include <iostream>
#include "utils/RGBA.h"
#include "utils/SceneParser.h"
#include "raytracer/RayTracer.hxx"

template<typename PointerType>
struct PlaneView {
    field(Data, static_cast<PointerType>(nullptr));
    field(RowSize, 0_z);

public:
    auto operator[](std::integral auto y) const {
        return Data + y * RowSize;
    }
};

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);


    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addPositionalArgument("config", "Path of the config file.");
    parser.process(a);

    auto positionalArgs = parser.positionalArguments();
    if (positionalArgs.size() != 1) {
        std::cerr << "not enough arguments for the ray tracer, please run with --help to see the manual" << std::endl;
        a.exit(1);
        return 1;
    }

    QSettings settings( positionalArgs[0], QSettings::IniFormat );
    QString iScenePath = settings.value("IO/scene").toString();
    QString oImagePath = settings.value("IO/output").toString();

    RenderData metaData;
    bool success = SceneParser::parse(iScenePath.toStdString(), metaData);

    if (!success) {
        std::cerr << "error loading scene: " << iScenePath.toStdString() << std::endl;
        a.exit(1);
        return 1;
    }

    int width = settings.value("Canvas/width").toInt();
    int height = settings.value("Canvas/height").toInt();

    RayTracer::Config::enableShadow = settings.value("Feature/shadows").toBool();
    RayTracer::Config::enableReflection = settings.value("Feature/reflect").toBool();
    RayTracer::Config::enableRefraction = settings.value("Feature/refract").toBool();
    RayTracer::Config::enableTextureMap = settings.value("Feature/texture").toBool();

    RayTracer::Config::enableParallelism = settings.value("Feature/parallel").toBool();
    RayTracer::Config::enableSuperSample = settings.value("Feature/super-sample").toBool();
    RayTracer::Config::enableAcceleration = settings.value("Feature/acceleration").toBool();
    RayTracer::Config::enableDepthOfField = settings.value("Feature/depthoffield").toBool();

    QImage image = QImage(width, height, QImage::Format_RGBX8888);
    image.fill(Qt::black);

    auto data = reinterpret_cast<RGBA*>(image.bits());
    auto SupersamplingExponent = 2;
    
    try {
        RayTracer::Draw(PlaneView<decltype(data)>{ .Data = data, .RowSize = width }, RayTracer::Render(height, width, SupersamplingExponent, metaData));
    }
    catch (std::exception& Error) {
        std::cerr << Error.what() << std::endl;
    }

    success = image.save(oImagePath);
    if (!success) {
        image.save(oImagePath, "PNG");
    }

    if (success) {
        std::cout << "Save rendered image to " << oImagePath.toStdString() << std::endl;
    } else {
        std::cerr << "Error: failed to save image to " << oImagePath.toStdString() << std::endl;
    }
    a.exit();
    return 0;
}
