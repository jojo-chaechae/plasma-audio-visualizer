#pragma once

#include <QColor>
#include <QImage>
#include <QQuickRhiItem>
#include <QVariantList>
#include <QVector>

#include <memory>

#include <rhi/qrhi.h>

class QRhi;
class QRhiBuffer;
class QRhiGraphicsPipeline;
class QRhiRenderPassDescriptor;
class QRhiSampler;
class QRhiShaderResourceBindings;
class QRhiTexture;

class SpectrumItemRenderer : public QQuickRhiItemRenderer
{
public:
    SpectrumItemRenderer();

    void initialize(QRhiCommandBuffer *cb) override;
    void synchronize(QQuickRhiItem *item) override;
    void render(QRhiCommandBuffer *cb) override;

private:
    SpectrumItemRenderer(const SpectrumItemRenderer &) = delete;
    SpectrumItemRenderer &operator=(const SpectrumItemRenderer &) = delete;
    SpectrumItemRenderer(SpectrumItemRenderer &&) = delete;
    SpectrumItemRenderer &operator=(SpectrumItemRenderer &&) = delete;

    void resetResources();
    bool ensureResources();
    bool ensurePipeline();
    QImage buildGradientLut(const QVariantList &colors, bool smooth) const;

    QRhi *m_rhi = nullptr;
    QRhiRenderPassDescriptor *m_pipelineRenderPass = nullptr;
    int m_pipelineSampleCount = 1;
    QRhiTexture::Format m_pipelineFormat = QRhiTexture::RGBA8;

    std::unique_ptr<QRhiBuffer> m_vertexBuffer;
    std::unique_ptr<QRhiBuffer> m_uniformBuffer;
    std::unique_ptr<QRhiTexture> m_spectrumTexture;
    std::unique_ptr<QRhiTexture> m_activeLut;
    std::unique_ptr<QRhiTexture> m_waveFillLut;
    std::unique_ptr<QRhiTexture> m_inactiveLut;
    std::unique_ptr<QRhiSampler> m_spectrumSampler;
    std::unique_ptr<QRhiSampler> m_colorSampler;
    std::unique_ptr<QRhiShaderResourceBindings> m_shaderResources;
    std::unique_ptr<QRhiGraphicsPipeline> m_pipeline;

    QVector<float> m_values;
    QVariantList m_gradientColors;
    QVariantList m_waveFillColors;
    QVariantList m_inactiveColors;

    quint64 m_seenValuesRevision = 0;
    quint64 m_seenGradientRevision = 0;
    quint64 m_seenWaveFillRevision = 0;
    quint64 m_seenInactiveRevision = 0;

    bool m_smoothGradient = true;
    bool m_waveFillSmoothGradient = true;
    bool m_inactiveSmoothGradient = true;
    int m_gradientOrientation = 1;
    int m_waveFillOrientation = 1;
    int m_inactiveOrientation = 1;
    int m_style = 0;
    int m_waveMode = 0;
    bool m_centered = false;
    bool m_rounded = false;
    bool m_circleMode = false;
    bool m_fillWave = false;
    bool m_drawInactive = false;
    bool m_waveform = false;
    bool m_waveSimulateWaveform = false;
    bool m_circleModeFill = false;
    float m_circleModeSize = 0.5f;
    float m_barWidth = 2.0f;
    float m_spacing = 5.0f;
    float m_blockHeight = 2.0f;
    float m_blockSpacing = 2.0f;
    float m_itemWidth = 0.0f;
    float m_itemHeight = 0.0f;

    bool m_spectrumDirty = true;
    bool m_activeLutDirty = true;
    bool m_waveFillLutDirty = true;
    bool m_inactiveLutDirty = true;
    bool m_vertexBufferDirty = true;
};

class SpectrumItem : public QQuickRhiItem
{
    Q_OBJECT
    Q_PROPERTY(QVariantList values READ values WRITE setValues NOTIFY valuesChanged FINAL)
    Q_PROPERTY(QVariantList gradientColors READ gradientColors WRITE setGradientColors NOTIFY gradientColorsChanged FINAL)
    Q_PROPERTY(QVariantList waveFillColors READ waveFillColors WRITE setWaveFillColors NOTIFY waveFillColorsChanged FINAL)
    Q_PROPERTY(QVariantList inactiveColors READ inactiveColors WRITE setInactiveColors NOTIFY inactiveColorsChanged FINAL)
    Q_PROPERTY(bool smoothGradient READ smoothGradient WRITE setSmoothGradient NOTIFY smoothGradientChanged FINAL)
    Q_PROPERTY(bool waveFillSmoothGradient READ waveFillSmoothGradient WRITE setWaveFillSmoothGradient NOTIFY waveFillSmoothGradientChanged FINAL)
    Q_PROPERTY(bool inactiveSmoothGradient READ inactiveSmoothGradient WRITE setInactiveSmoothGradient NOTIFY inactiveSmoothGradientChanged FINAL)
    Q_PROPERTY(int gradientOrientation READ gradientOrientation WRITE setGradientOrientation NOTIFY gradientOrientationChanged FINAL)
    Q_PROPERTY(int waveFillOrientation READ waveFillOrientation WRITE setWaveFillOrientation NOTIFY waveFillOrientationChanged FINAL)
    Q_PROPERTY(int inactiveOrientation READ inactiveOrientation WRITE setInactiveOrientation NOTIFY inactiveOrientationChanged FINAL)
    Q_PROPERTY(int style READ style WRITE setStyle NOTIFY styleChanged FINAL)
    Q_PROPERTY(int waveMode READ waveMode WRITE setWaveMode NOTIFY waveModeChanged FINAL)
    Q_PROPERTY(bool centered READ centered WRITE setCentered NOTIFY centeredChanged FINAL)
    Q_PROPERTY(bool rounded READ rounded WRITE setRounded NOTIFY roundedChanged FINAL)
    Q_PROPERTY(bool circleMode READ circleMode WRITE setCircleMode NOTIFY circleModeChanged FINAL)
    Q_PROPERTY(bool fillWave READ fillWave WRITE setFillWave NOTIFY fillWaveChanged FINAL)
    Q_PROPERTY(bool drawInactive READ drawInactive WRITE setDrawInactive NOTIFY drawInactiveChanged FINAL)
    Q_PROPERTY(bool waveform READ waveform WRITE setWaveform NOTIFY waveformChanged FINAL)
    Q_PROPERTY(bool waveSimulateWaveform READ waveSimulateWaveform WRITE setWaveSimulateWaveform NOTIFY waveSimulateWaveformChanged FINAL)
    Q_PROPERTY(bool circleModeFill READ circleModeFill WRITE setCircleModeFill NOTIFY circleModeFillChanged FINAL)
    Q_PROPERTY(qreal circleModeSize READ circleModeSize WRITE setCircleModeSize NOTIFY circleModeSizeChanged FINAL)
    Q_PROPERTY(int barWidth READ barWidth WRITE setBarWidth NOTIFY barWidthChanged FINAL)
    Q_PROPERTY(int spacing READ spacing WRITE setSpacing NOTIFY spacingChanged FINAL)
    Q_PROPERTY(int blockHeight READ blockHeight WRITE setBlockHeight NOTIFY blockHeightChanged FINAL)
    Q_PROPERTY(int blockSpacing READ blockSpacing WRITE setBlockSpacing NOTIFY blockSpacingChanged FINAL)

public:
    explicit SpectrumItem(QQuickItem *parent = nullptr);
    QQuickRhiItemRenderer *createRenderer() override;

    QVariantList values() const { return m_values; }
    void setValues(const QVariantList &values);

    QVariantList gradientColors() const { return m_gradientColors; }
    void setGradientColors(const QVariantList &colors);

    QVariantList waveFillColors() const { return m_waveFillColors; }
    void setWaveFillColors(const QVariantList &colors);

    QVariantList inactiveColors() const { return m_inactiveColors; }
    void setInactiveColors(const QVariantList &colors);

    bool smoothGradient() const { return m_smoothGradient; }
    void setSmoothGradient(bool smooth);

    bool waveFillSmoothGradient() const { return m_waveFillSmoothGradient; }
    void setWaveFillSmoothGradient(bool smooth);

    bool inactiveSmoothGradient() const { return m_inactiveSmoothGradient; }
    void setInactiveSmoothGradient(bool smooth);

    int gradientOrientation() const { return m_gradientOrientation; }
    void setGradientOrientation(int orientation);

    int waveFillOrientation() const { return m_waveFillOrientation; }
    void setWaveFillOrientation(int orientation);

    int inactiveOrientation() const { return m_inactiveOrientation; }
    void setInactiveOrientation(int orientation);

    int style() const { return m_style; }
    void setStyle(int style);

    int waveMode() const { return m_waveMode; }
    void setWaveMode(int waveMode);

    bool centered() const { return m_centered; }
    void setCentered(bool centered);

    bool rounded() const { return m_rounded; }
    void setRounded(bool rounded);

    bool circleMode() const { return m_circleMode; }
    void setCircleMode(bool circleMode);

    bool fillWave() const { return m_fillWave; }
    void setFillWave(bool fillWave);

    bool drawInactive() const { return m_drawInactive; }
    void setDrawInactive(bool drawInactive);

    bool waveform() const { return m_waveform; }
    void setWaveform(bool waveform);

    bool waveSimulateWaveform() const { return m_waveSimulateWaveform; }
    void setWaveSimulateWaveform(bool simulate);

    bool circleModeFill() const { return m_circleModeFill; }
    void setCircleModeFill(bool fill);

    qreal circleModeSize() const { return m_circleModeSize; }
    void setCircleModeSize(qreal size);

    int barWidth() const { return m_barWidth; }
    void setBarWidth(int width);

    int spacing() const { return m_spacing; }
    void setSpacing(int spacing);

    int blockHeight() const { return m_blockHeight; }
    void setBlockHeight(int height);

    int blockSpacing() const { return m_blockSpacing; }
    void setBlockSpacing(int spacing);

    quint64 valuesRevision() const { return m_valuesRevision; }
    quint64 gradientColorsRevision() const { return m_gradientColorsRevision; }
    quint64 waveFillColorsRevision() const { return m_waveFillColorsRevision; }
    quint64 inactiveColorsRevision() const { return m_inactiveColorsRevision; }

signals:
    void valuesChanged();
    void gradientColorsChanged();
    void waveFillColorsChanged();
    void inactiveColorsChanged();
    void smoothGradientChanged();
    void waveFillSmoothGradientChanged();
    void inactiveSmoothGradientChanged();
    void gradientOrientationChanged();
    void waveFillOrientationChanged();
    void inactiveOrientationChanged();
    void styleChanged();
    void waveModeChanged();
    void centeredChanged();
    void roundedChanged();
    void circleModeChanged();
    void fillWaveChanged();
    void drawInactiveChanged();
    void waveformChanged();
    void waveSimulateWaveformChanged();
    void circleModeFillChanged();
    void circleModeSizeChanged();
    void barWidthChanged();
    void spacingChanged();
    void blockHeightChanged();
    void blockSpacingChanged();

private:
    QVariantList m_values;
    QVariantList m_gradientColors;
    QVariantList m_waveFillColors;
    QVariantList m_inactiveColors;
    quint64 m_valuesRevision = 0;
    quint64 m_gradientColorsRevision = 0;
    quint64 m_waveFillColorsRevision = 0;
    quint64 m_inactiveColorsRevision = 0;
    bool m_smoothGradient = true;
    bool m_waveFillSmoothGradient = true;
    bool m_inactiveSmoothGradient = true;
    int m_gradientOrientation = 1;
    int m_waveFillOrientation = 1;
    int m_inactiveOrientation = 1;
    int m_style = 0;
    int m_waveMode = 0;
    bool m_centered = false;
    bool m_rounded = false;
    bool m_circleMode = false;
    bool m_fillWave = false;
    bool m_drawInactive = false;
    bool m_waveform = false;
    bool m_waveSimulateWaveform = false;
    bool m_circleModeFill = false;
    qreal m_circleModeSize = 0.5;
    int m_barWidth = 2;
    int m_spacing = 5;
    int m_blockHeight = 2;
    int m_blockSpacing = 2;
};
