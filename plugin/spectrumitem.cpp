#include "spectrumitem.h"

#include <QDebug>
#include <QFile>
#include <QMatrix4x4>
#include <QSize>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace {

constexpr int kLutWidth = 1024;

struct Uniforms {
    float clip[16];
    float p0[4];
    float p1[4];
    float p2[4];
    float p3[4];
    float p4[4];
    float p5[4];
};

float lerp(float a, float b, float t)
{
    return a + (b - a) * t;
}

QShader loadShader(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "SpectrumItem: failed to open shader" << path;
        return {};
    }
    return QShader::fromSerialized(file.readAll());
}

} // namespace

SpectrumItemRenderer::SpectrumItemRenderer()
    : m_vertexBuffer()
    , m_uniformBuffer()
    , m_spectrumTexture()
    , m_activeLut()
    , m_waveFillLut()
    , m_inactiveLut()
    , m_spectrumSampler()
    , m_colorSampler()
    , m_shaderResources()
    , m_pipeline()
    , m_values()
    , m_gradientColors()
    , m_waveFillColors()
    , m_inactiveColors()
{
}

void SpectrumItemRenderer::resetResources()
{
    m_pipeline.reset();
    m_shaderResources.reset();
    m_inactiveLut.reset();
    m_waveFillLut.reset();
    m_activeLut.reset();
    m_spectrumTexture.reset();
    m_colorSampler.reset();
    m_spectrumSampler.reset();
    m_uniformBuffer.reset();
    m_vertexBuffer.reset();

    m_pipelineRenderPass = nullptr;
    m_pipelineSampleCount = 1;
    m_pipelineFormat = QRhiTexture::RGBA8;

    m_spectrumDirty = true;
    m_activeLutDirty = true;
    m_waveFillLutDirty = true;
    m_inactiveLutDirty = true;
    m_vertexBufferDirty = true;
}

void SpectrumItemRenderer::initialize(QRhiCommandBuffer *cb)
{
    Q_UNUSED(cb);

    if (m_rhi != rhi()) {
        m_rhi = rhi();
        resetResources();
    }
}

void SpectrumItemRenderer::synchronize(QQuickRhiItem *rhiItem)
{
    auto *item = static_cast<SpectrumItem *>(rhiItem);

    if (m_seenValuesRevision != item->valuesRevision()) {
        m_seenValuesRevision = item->valuesRevision();
        const QVariantList values = item->values();
        m_values.resize(values.size());
        for (qsizetype i = 0; i < values.size(); ++i) {
            m_values[i] = values.at(i).toFloat();
        }
        m_spectrumDirty = true;
    }

    if (m_seenGradientRevision != item->gradientColorsRevision()) {
        m_seenGradientRevision = item->gradientColorsRevision();
        m_gradientColors = item->gradientColors();
        m_activeLutDirty = true;
    }

    if (m_seenWaveFillRevision != item->waveFillColorsRevision()) {
        m_seenWaveFillRevision = item->waveFillColorsRevision();
        m_waveFillColors = item->waveFillColors();
        m_waveFillLutDirty = true;
    }

    if (m_seenInactiveRevision != item->inactiveColorsRevision()) {
        m_seenInactiveRevision = item->inactiveColorsRevision();
        m_inactiveColors = item->inactiveColors();
        m_inactiveLutDirty = true;
    }

    if (m_smoothGradient != item->smoothGradient()) {
        m_smoothGradient = item->smoothGradient();
        m_activeLutDirty = true;
    }
    if (m_waveFillSmoothGradient != item->waveFillSmoothGradient()) {
        m_waveFillSmoothGradient = item->waveFillSmoothGradient();
        m_waveFillLutDirty = true;
    }
    if (m_inactiveSmoothGradient != item->inactiveSmoothGradient()) {
        m_inactiveSmoothGradient = item->inactiveSmoothGradient();
        m_inactiveLutDirty = true;
    }
    if (m_gradientOrientation != item->gradientOrientation()) {
        m_gradientOrientation = item->gradientOrientation();
        m_activeLutDirty = true;
    }
    if (m_waveFillOrientation != item->waveFillOrientation()) {
        m_waveFillOrientation = item->waveFillOrientation();
        m_waveFillLutDirty = true;
    }
    if (m_inactiveOrientation != item->inactiveOrientation()) {
        m_inactiveOrientation = item->inactiveOrientation();
        m_inactiveLutDirty = true;
    }

    m_style = item->style();
    m_waveMode = item->waveMode();
    m_centered = item->centered();
    m_rounded = item->rounded();
    m_circleMode = item->circleMode();
    m_fillWave = item->fillWave();
    m_drawInactive = item->drawInactive();
    m_waveform = item->waveform();
    m_waveSimulateWaveform = item->waveSimulateWaveform();
    m_circleModeFill = item->circleModeFill();
    m_circleModeSize = static_cast<float>(item->circleModeSize());
    m_barWidth = static_cast<float>(item->barWidth());
    m_spacing = static_cast<float>(item->spacing());
    m_blockHeight = static_cast<float>(item->blockHeight());
    m_blockSpacing = static_cast<float>(item->blockSpacing());
    m_itemWidth = static_cast<float>(item->width());
    m_itemHeight = static_cast<float>(item->height());
}

bool SpectrumItemRenderer::ensureResources()
{
    if (!m_rhi) {
        return false;
    }

    if (!m_vertexBuffer) {
        m_vertexBuffer.reset(m_rhi->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer, 3 * 4 * sizeof(float)));
        if (!m_vertexBuffer->create()) {
            m_vertexBuffer.reset();
            return false;
        }
        m_vertexBufferDirty = true;
    }

    if (!m_uniformBuffer) {
        m_uniformBuffer.reset(m_rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(Uniforms)));
        if (!m_uniformBuffer->create()) {
            m_uniformBuffer.reset();
            return false;
        }
    }

    if (!m_spectrumSampler) {
        m_spectrumSampler.reset(m_rhi->newSampler(QRhiSampler::Nearest,
                                                  QRhiSampler::Nearest,
                                                  QRhiSampler::None,
                                                  QRhiSampler::ClampToEdge,
                                                  QRhiSampler::ClampToEdge));
        if (!m_spectrumSampler->create()) {
            m_spectrumSampler.reset();
            return false;
        }
    }

    if (!m_colorSampler) {
        m_colorSampler.reset(m_rhi->newSampler(QRhiSampler::Linear,
                                               QRhiSampler::Linear,
                                               QRhiSampler::None,
                                               QRhiSampler::ClampToEdge,
                                               QRhiSampler::ClampToEdge));
        if (!m_colorSampler->create()) {
            m_colorSampler.reset();
            return false;
        }
    }

    const int spectrumWidth = std::max(1, static_cast<int>(m_values.size()));
    if (!m_spectrumTexture || m_spectrumTexture->pixelSize().width() != spectrumWidth) {
        m_spectrumTexture.reset(m_rhi->newTexture(QRhiTexture::R32F, QSize(spectrumWidth, 1)));
        if (!m_spectrumTexture->create()) {
            m_spectrumTexture.reset();
            return false;
        }
        m_spectrumDirty = true;
        m_shaderResources.reset();
    }

    if (!m_activeLut) {
        m_activeLut.reset(m_rhi->newTexture(QRhiTexture::RGBA8, QSize(kLutWidth, 1)));
        if (!m_activeLut->create()) {
            m_activeLut.reset();
            return false;
        }
        m_activeLutDirty = true;
        m_shaderResources.reset();
    }

    if (!m_waveFillLut) {
        m_waveFillLut.reset(m_rhi->newTexture(QRhiTexture::RGBA8, QSize(kLutWidth, 1)));
        if (!m_waveFillLut->create()) {
            m_waveFillLut.reset();
            return false;
        }
        m_waveFillLutDirty = true;
        m_shaderResources.reset();
    }

    if (!m_inactiveLut) {
        m_inactiveLut.reset(m_rhi->newTexture(QRhiTexture::RGBA8, QSize(kLutWidth, 1)));
        if (!m_inactiveLut->create()) {
            m_inactiveLut.reset();
            return false;
        }
        m_inactiveLutDirty = true;
        m_shaderResources.reset();
    }

    if (!m_shaderResources) {
        m_shaderResources.reset(m_rhi->newShaderResourceBindings());
        m_shaderResources->setBindings({
            QRhiShaderResourceBinding::uniformBuffer(0,
                                                     QRhiShaderResourceBinding::VertexStage
                                                         | QRhiShaderResourceBinding::FragmentStage,
                                                     m_uniformBuffer.get()),
            QRhiShaderResourceBinding::sampledTexture(1,
                                                      QRhiShaderResourceBinding::FragmentStage,
                                                      m_spectrumTexture.get(),
                                                      m_spectrumSampler.get()),
            QRhiShaderResourceBinding::sampledTexture(2,
                                                      QRhiShaderResourceBinding::FragmentStage,
                                                      m_activeLut.get(),
                                                      m_colorSampler.get()),
            QRhiShaderResourceBinding::sampledTexture(3,
                                                      QRhiShaderResourceBinding::FragmentStage,
                                                      m_waveFillLut.get(),
                                                      m_colorSampler.get()),
            QRhiShaderResourceBinding::sampledTexture(4,
                                                      QRhiShaderResourceBinding::FragmentStage,
                                                      m_inactiveLut.get(),
                                                      m_colorSampler.get()),
        });
        if (!m_shaderResources->create()) {
            m_shaderResources.reset();
            return false;
        }
        m_pipeline.reset();
    }

    return true;
}

bool SpectrumItemRenderer::ensurePipeline()
{
    QRhiRenderTarget *rt = renderTarget();
    if (!rt) {
        return false;
    }

    QRhiTexture *finalTexture = rt->sampleCount() > 1 ? resolveTexture() : colorTexture();
    if (!finalTexture) {
        return false;
    }

    if (m_pipeline
        && (m_pipelineRenderPass != rt->renderPassDescriptor()
            || m_pipelineSampleCount != rt->sampleCount()
            || m_pipelineFormat != finalTexture->format())) {
        m_pipeline.reset();
    }

    if (m_pipeline) {
        return true;
    }

    if (!m_shaderResources) {
        return false;
    }

    const QShader vertexShader = loadShader(QStringLiteral(":/spectrum/shaders/spectrum.vert.qsb"));
    const QShader fragmentShader = loadShader(QStringLiteral(":/spectrum/shaders/spectrum.frag.qsb"));
    if (!vertexShader.isValid() || !fragmentShader.isValid()) {
        return false;
    }

    m_pipeline.reset(m_rhi->newGraphicsPipeline());
    m_pipeline->setShaderStages({
        { QRhiShaderStage::Vertex, vertexShader },
        { QRhiShaderStage::Fragment, fragmentShader },
    });

    QRhiVertexInputLayout inputLayout;
    inputLayout.setBindings({
        { 4 * static_cast<quint32>(sizeof(float)) },
    });
    inputLayout.setAttributes({
        { 0, 0, QRhiVertexInputAttribute::Float2, 0 },
        { 0, 1, QRhiVertexInputAttribute::Float2, 2 * static_cast<quint32>(sizeof(float)) },
    });
    m_pipeline->setVertexInputLayout(inputLayout);
    m_pipeline->setSampleCount(rt->sampleCount());
    m_pipeline->setShaderResourceBindings(m_shaderResources.get());
    m_pipeline->setRenderPassDescriptor(rt->renderPassDescriptor());
    m_pipeline->setTopology(QRhiGraphicsPipeline::Triangles);
    m_pipeline->setCullMode(QRhiGraphicsPipeline::None);
    m_pipeline->setDepthTest(false);
    m_pipeline->setDepthWrite(false);

    if (!m_pipeline->create()) {
        m_pipeline.reset();
        return false;
    }

    m_pipelineRenderPass = rt->renderPassDescriptor();
    m_pipelineSampleCount = rt->sampleCount();
    m_pipelineFormat = finalTexture->format();
    return true;
}

QImage SpectrumItemRenderer::buildGradientLut(const QVariantList &colors, bool smooth) const
{
    QImage image(kLutWidth, 1, QImage::Format_RGBA8888);
    image.fill(Qt::transparent);

    QVector<QColor> stops;
    stops.reserve(colors.size());
    for (const QVariant &value : colors) {
        const QColor color = value.value<QColor>();
        if (color.isValid()) {
            stops.append(color);
        }
    }
    if (stops.isEmpty()) {
        return image;
    }

    const qsizetype count = stops.size();
    for (int x = 0; x < kLutWidth; ++x) {
        const float t = kLutWidth > 1 ? static_cast<float>(x) / static_cast<float>(kLutWidth - 1) : 0.0f;
        QColor color;

        if (count == 1) {
            color = stops.first();
        } else if (smooth) {
            const float position = t * static_cast<float>(count - 1);
            const qsizetype index = std::clamp(static_cast<qsizetype>(std::floor(position)), qsizetype(0), count - 1);
            if (index >= count - 1) {
                color = stops.last();
            } else {
                const float fraction = position - static_cast<float>(index);
                const QColor &a = stops.at(index);
                const QColor &b = stops.at(index + 1);
                color = QColor::fromRgbF(lerp(static_cast<float>(a.redF()), static_cast<float>(b.redF()), fraction),
                                         lerp(static_cast<float>(a.greenF()), static_cast<float>(b.greenF()), fraction),
                                         lerp(static_cast<float>(a.blueF()), static_cast<float>(b.blueF()), fraction),
                                         lerp(static_cast<float>(a.alphaF()), static_cast<float>(b.alphaF()), fraction));
            }
        } else {
            const qsizetype index = std::clamp(static_cast<qsizetype>(std::floor(t * static_cast<float>(count))),
                                               qsizetype(0),
                                               count - 1);
            color = stops.at(index);
        }

        image.setPixelColor(x, 0, color);
    }

    return image;
}

void SpectrumItemRenderer::render(QRhiCommandBuffer *cb)
{
    if (!m_rhi) {
        return;
    }

    QRhiRenderTarget *rt = renderTarget();
    if (!rt || rt->pixelSize().isEmpty()) {
        return;
    }

    if (!ensureResources() || !ensurePipeline()) {
        return;
    }

    Uniforms uniforms{};
    const QMatrix4x4 clip = m_rhi->clipSpaceCorrMatrix();
    std::memcpy(uniforms.clip, clip.constData(), sizeof(uniforms.clip));

    uniforms.p0[0] = m_itemWidth;
    uniforms.p0[1] = m_itemHeight;
    uniforms.p0[2] = static_cast<float>(rt->pixelSize().width()) / std::max(m_itemWidth, 1.0f);
    uniforms.p0[3] = static_cast<float>(m_values.size());

    uniforms.p1[0] = m_barWidth;
    uniforms.p1[1] = m_spacing;
    uniforms.p1[2] = m_centered ? 1.0f : 0.0f;
    uniforms.p1[3] = m_rounded ? 1.0f : 0.0f;

    uniforms.p2[0] = m_circleMode ? 1.0f : 0.0f;
    uniforms.p2[1] = m_circleModeSize;
    uniforms.p2[2] = static_cast<float>(m_style);
    uniforms.p2[3] = static_cast<float>(m_waveMode);

    uniforms.p3[0] = m_fillWave ? 1.0f : 0.0f;
    uniforms.p3[1] = m_drawInactive ? 1.0f : 0.0f;
    uniforms.p3[2] = m_blockHeight;
    uniforms.p3[3] = m_blockSpacing;

    uniforms.p4[0] = static_cast<float>(m_gradientOrientation);
    uniforms.p4[1] = m_waveform ? 1.0f : 0.0f;
    uniforms.p4[2] = m_waveSimulateWaveform ? 1.0f : 0.0f;
    uniforms.p4[3] = m_circleModeFill ? 1.0f : 0.0f;

    uniforms.p5[0] = static_cast<float>(m_waveFillOrientation);
    uniforms.p5[1] = static_cast<float>(m_inactiveOrientation);
    uniforms.p5[2] = 0.0f;
    uniforms.p5[3] = 0.0f;

    QRhiResourceUpdateBatch *updates = m_rhi->nextResourceUpdateBatch();
    updates->updateDynamicBuffer(m_uniformBuffer.get(), 0, sizeof(Uniforms), &uniforms);

    if (m_vertexBufferDirty) {
        static const float vertexData[] = {
            -1.0f, -1.0f, 0.0f, 1.0f,
            3.0f, -1.0f, 2.0f, 1.0f,
            -1.0f, 3.0f, 0.0f, -1.0f,
        };
        updates->uploadStaticBuffer(m_vertexBuffer.get(), vertexData);
        m_vertexBufferDirty = false;
    }

    if (m_spectrumDirty) {
        const int width = m_spectrumTexture->pixelSize().width();
        QByteArray data;
        if (m_values.isEmpty()) {
            const float zero = 0.0f;
            data = QByteArray(reinterpret_cast<const char *>(&zero), static_cast<qsizetype>(sizeof(zero)));
        } else {
            data = QByteArray(reinterpret_cast<const char *>(m_values.constData()),
                              m_values.size() * static_cast<qsizetype>(sizeof(float)));
        }

        QRhiTextureSubresourceUploadDescription description(data);
        description.setSourceSize(QSize(width, 1));
        description.setDataStride(static_cast<quint32>(width * static_cast<int>(sizeof(float))));
        updates->uploadTexture(m_spectrumTexture.get(),
                               QRhiTextureUploadDescription(QRhiTextureUploadEntry(0, 0, description)));
        m_spectrumDirty = false;
    }

    if (m_activeLutDirty) {
        updates->uploadTexture(m_activeLut.get(), buildGradientLut(m_gradientColors, m_smoothGradient));
        m_activeLutDirty = false;
    }

    if (m_waveFillLutDirty) {
        updates->uploadTexture(m_waveFillLut.get(), buildGradientLut(m_waveFillColors, m_waveFillSmoothGradient));
        m_waveFillLutDirty = false;
    }

    if (m_inactiveLutDirty) {
        updates->uploadTexture(m_inactiveLut.get(), buildGradientLut(m_inactiveColors, m_inactiveSmoothGradient));
        m_inactiveLutDirty = false;
    }

    cb->beginPass(rt, QColor::fromRgbF(0.0f, 0.0f, 0.0f, 0.0f), { 1.0f, 0 }, updates);
    cb->setGraphicsPipeline(m_pipeline.get());
    cb->setViewport(QRhiViewport(0.0f,
                                 0.0f,
                                 static_cast<float>(rt->pixelSize().width()),
                                 static_cast<float>(rt->pixelSize().height())));
    cb->setShaderResources(m_shaderResources.get());
    const QRhiCommandBuffer::VertexInput vertexInput(m_vertexBuffer.get(), 0);
    cb->setVertexInput(0, 1, &vertexInput);
    cb->draw(3);
    cb->endPass();
}

QQuickRhiItemRenderer *SpectrumItem::createRenderer()
{
    return new SpectrumItemRenderer;
}

SpectrumItem::SpectrumItem(QQuickItem *parent)
    : QQuickRhiItem(parent)
    , m_values()
    , m_gradientColors()
    , m_waveFillColors()
    , m_inactiveColors()
{
}

void SpectrumItem::setValues(const QVariantList &values)
{
    if (m_values == values) {
        return;
    }
    m_values = values;
    ++m_valuesRevision;
    emit valuesChanged();
    update();
}

void SpectrumItem::setGradientColors(const QVariantList &colors)
{
    if (m_gradientColors == colors) {
        return;
    }
    m_gradientColors = colors;
    ++m_gradientColorsRevision;
    emit gradientColorsChanged();
    update();
}

void SpectrumItem::setWaveFillColors(const QVariantList &colors)
{
    if (m_waveFillColors == colors) {
        return;
    }
    m_waveFillColors = colors;
    ++m_waveFillColorsRevision;
    emit waveFillColorsChanged();
    update();
}

void SpectrumItem::setInactiveColors(const QVariantList &colors)
{
    if (m_inactiveColors == colors) {
        return;
    }
    m_inactiveColors = colors;
    ++m_inactiveColorsRevision;
    emit inactiveColorsChanged();
    update();
}

void SpectrumItem::setSmoothGradient(bool smooth)
{
    if (m_smoothGradient == smooth) {
        return;
    }
    m_smoothGradient = smooth;
    emit smoothGradientChanged();
    update();
}

void SpectrumItem::setWaveFillSmoothGradient(bool smooth)
{
    if (m_waveFillSmoothGradient == smooth) {
        return;
    }
    m_waveFillSmoothGradient = smooth;
    emit waveFillSmoothGradientChanged();
    update();
}

void SpectrumItem::setInactiveSmoothGradient(bool smooth)
{
    if (m_inactiveSmoothGradient == smooth) {
        return;
    }
    m_inactiveSmoothGradient = smooth;
    emit inactiveSmoothGradientChanged();
    update();
}

void SpectrumItem::setGradientOrientation(int orientation)
{
    if (m_gradientOrientation == orientation) {
        return;
    }
    m_gradientOrientation = orientation;
    emit gradientOrientationChanged();
    update();
}

void SpectrumItem::setWaveFillOrientation(int orientation)
{
    if (m_waveFillOrientation == orientation) {
        return;
    }
    m_waveFillOrientation = orientation;
    emit waveFillOrientationChanged();
    update();
}

void SpectrumItem::setInactiveOrientation(int orientation)
{
    if (m_inactiveOrientation == orientation) {
        return;
    }
    m_inactiveOrientation = orientation;
    emit inactiveOrientationChanged();
    update();
}

void SpectrumItem::setStyle(int style)
{
    if (m_style == style) {
        return;
    }
    m_style = style;
    emit styleChanged();
    update();
}

void SpectrumItem::setWaveMode(int waveMode)
{
    if (m_waveMode == waveMode) {
        return;
    }
    m_waveMode = waveMode;
    emit waveModeChanged();
    update();
}

void SpectrumItem::setCentered(bool centered)
{
    if (m_centered == centered) {
        return;
    }
    m_centered = centered;
    emit centeredChanged();
    update();
}

void SpectrumItem::setRounded(bool rounded)
{
    if (m_rounded == rounded) {
        return;
    }
    m_rounded = rounded;
    emit roundedChanged();
    update();
}

void SpectrumItem::setCircleMode(bool circleMode)
{
    if (m_circleMode == circleMode) {
        return;
    }
    m_circleMode = circleMode;
    emit circleModeChanged();
    update();
}

void SpectrumItem::setFillWave(bool fillWave)
{
    if (m_fillWave == fillWave) {
        return;
    }
    m_fillWave = fillWave;
    emit fillWaveChanged();
    update();
}

void SpectrumItem::setDrawInactive(bool drawInactive)
{
    if (m_drawInactive == drawInactive) {
        return;
    }
    m_drawInactive = drawInactive;
    emit drawInactiveChanged();
    update();
}

void SpectrumItem::setWaveform(bool waveform)
{
    if (m_waveform == waveform) {
        return;
    }
    m_waveform = waveform;
    emit waveformChanged();
    update();
}

void SpectrumItem::setWaveSimulateWaveform(bool simulate)
{
    if (m_waveSimulateWaveform == simulate) {
        return;
    }
    m_waveSimulateWaveform = simulate;
    emit waveSimulateWaveformChanged();
    update();
}

void SpectrumItem::setCircleModeFill(bool fill)
{
    if (m_circleModeFill == fill) {
        return;
    }
    m_circleModeFill = fill;
    emit circleModeFillChanged();
    update();
}

void SpectrumItem::setCircleModeSize(qreal size)
{
    if (m_circleModeSize == size) {
        return;
    }
    m_circleModeSize = size;
    emit circleModeSizeChanged();
    update();
}

void SpectrumItem::setBarWidth(int width)
{
    if (m_barWidth == width) {
        return;
    }
    m_barWidth = width;
    emit barWidthChanged();
    update();
}

void SpectrumItem::setSpacing(int spacing)
{
    if (m_spacing == spacing) {
        return;
    }
    m_spacing = spacing;
    emit spacingChanged();
    update();
}

void SpectrumItem::setBlockHeight(int height)
{
    if (m_blockHeight == height) {
        return;
    }
    m_blockHeight = height;
    emit blockHeightChanged();
    update();
}

void SpectrumItem::setBlockSpacing(int spacing)
{
    if (m_blockSpacing == spacing) {
        return;
    }
    m_blockSpacing = spacing;
    emit blockSpacingChanged();
    update();
}
