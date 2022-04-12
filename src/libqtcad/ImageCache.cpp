#include "qtcad/ImageCache.h"
#include "bu/ptbl.h"

namespace qtcad {

ImageCache::ImageCache()
{
    loadImage("DB5_MINORTYPE_BRLCAD_OTHER", ":/images/primitives/other.png");
    loadImage("DB5_MINORTYPE_BRLCAD_TOR", ":/images/primitives/tor.png");
    loadImage("DB5_MINORTYPE_BRLCAD_TGC", ":/images/primitives/tgc.png");
    loadImage("DB5_MINORTYPE_BRLCAD_ELL", ":/images/primitives/ell.png");
    loadImage("DB5_MINORTYPE_BRLCAD_ARB4", ":/images/primitives/arb4.png");
    loadImage("DB5_MINORTYPE_BRLCAD_ARB5", ":/images/primitives/arb5.png");
    loadImage("DB5_MINORTYPE_BRLCAD_ARB6", ":/images/primitives/arb6.png");
    loadImage("DB5_MINORTYPE_BRLCAD_ARB7", ":/images/primitives/arb7.png");
    loadImage("DB5_MINORTYPE_BRLCAD_ARB8", ":/images/primitives/arb8.png");
    setImage("DB5_MINORTYPE_BRLCAD_ARB8_OTHER", getImage("DB5_MINORTYPE_BRLCAD_OTHER"));
    loadImage("DB5_MINORTYPE_BRLCAD_ARS", ":/images/primitives/ars.png");
    loadImage("DB5_MINORTYPE_BRLCAD_HALF", ":/images/primitives/half.png");
    loadImage("DB5_MINORTYPE_BRLCAD_REC", ":/images/primitives/tgc.png");
    setImage("DB5_MINORTYPE_BRLCAD_POLY", getImage("DB5_MINORTYPE_BRLCAD_OTHER"));
    setImage("DB5_MINORTYPE_BRLCAD_BSPLINE", getImage("DB5_MINORTYPE_BRLCAD_OTHER"));
    loadImage("DB5_MINORTYPE_BRLCAD_SPH", ":/images/primitives/sph.png");
    loadImage("DB5_MINORTYPE_BRLCAD_NMG", ":/images/primitives/nmg.png");
    setImage("DB5_MINORTYPE_BRLCAD_EBM", getImage("DB5_MINORTYPE_BRLCAD_OTHER"));
    setImage("DB5_MINORTYPE_BRLCAD_VOL", getImage("DB5_MINORTYPE_BRLCAD_OTHER"));
    loadImage("DB5_MINORTYPE_BRLCAD_ARBN", ":/images/primitives/arbn.png");
    loadImage("DB5_MINORTYPE_BRLCAD_PIPE", ":/images/primitives/pipe.png");
    loadImage("DB5_MINORTYPE_BRLCAD_PARTICLE", ":/images/primitives/part.png");
    loadImage("DB5_MINORTYPE_BRLCAD_RPC", ":/images/primitives/rpc.png");
    loadImage("DB5_MINORTYPE_BRLCAD_RHC", ":/images/primitives/rhc.png");
    loadImage("DB5_MINORTYPE_BRLCAD_EPA", ":/images/primitives/epa.png");
    loadImage("DB5_MINORTYPE_BRLCAD_EHY", ":/images/primitives/ehy.png");
    loadImage("DB5_MINORTYPE_BRLCAD_ETO", ":/images/primitives/eto.png");
    setImage("DB5_MINORTYPE_BRLCAD_GRIP", getImage(":/images/primitives/other.png"));
    setImage("DB5_MINORTYPE_BRLCAD_JOINT", getImage(":/images/primitives/other.png"));
    setImage("DB5_MINORTYPE_BRLCAD_HF", getImage(":/images/primitives/other.png"));
    loadImage("DB5_MINORTYPE_BRLCAD_DSP", ":/images/primitives/dsp.png");
    loadImage("DB5_MINORTYPE_BRLCAD_SKETCH", ":/images/primitives/sketch.png");
    loadImage("DB5_MINORTYPE_BRLCAD_EXTRUDE", ":/images/primitives/extrude.png");
    setImage("DB5_MINORTYPE_BRLCAD_SUBMODEL", getImage(":/images/primitives/other.png"));
    setImage("DB5_MINORTYPE_BRLCAD_CLINE", getImage(":/images/primitives/other.png"));
    loadImage("DB5_MINORTYPE_BRLCAD_BOT", ":/images/primitives/bot.png");
    loadImage("G_REGION", ":/images/primitives/region.png");
    loadImage("G_AIR", ":/images/primitives/air.png");
    loadImage("G_AIR_REGION", ":/images/primitives/airregion.png");
    loadImage("G_ASSEMBLY", ":/images/primitives/assembly.png");
    setImage("DB5_MINORTYPE_BRLCAD_SUPERELL", getImage(":/images/primitives/other.png"));
    loadImage("DB5_MINORTYPE_BRLCAD_METABALL", ":/images/primitives/metaball.png");
    loadImage("DB5_MINORTYPE_BRLCAD_BREP", ":/images/primitives/brep.png");
    loadImage("DB5_MINORTYPE_BRLCAD_HYP", ":/images/primitives/hyp.png");
    setImage("DB5_MINORTYPE_BRLCAD_CONSTRAINT", getImage(":/images/primitives/other.png"));
    setImage("DB5_MINORTYPE_BRLCAD_REVOLVE", getImage(":/images/primitives/other.png"));
    setImage("DB5_MINORTYPE_BRLCAD_ANNOT", getImage(":/images/primitives/other.png"));
    setImage("DB5_MINORTYPE_BRLCAD_HRT", getImage(":/images/primitives/other.png"));
    loadImage("DB5_MINORTYPE_BRLCAD_INVALID", ":/images/primitives/invalid.png");
}

QImage ImageCache::getImage(const QString name)
{
    return images[name];
}

void ImageCache::setImage(const QString name, QImage image)
{
    images[name] = image;
}

void ImageCache::loadImage(QString name, QString filename)
{
    QImage image;
    image.load(filename);
    images[name] = image;
}

}
