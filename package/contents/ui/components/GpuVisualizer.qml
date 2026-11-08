import QtQuick
import org.kde.kirigami as Kirigami
import com.github.luisbocanegra.audiovisualizer.process
import "../code/utils.js" as Utils
import "../code/enum.js" as Enum

Item {
    id: root
    anchors.fill: parent

    property var visualizerState: ({})
    readonly property var s: visualizerState
    readonly property int barCount: s.values ? s.values.length : 0
    readonly property int spacing: s.visualizerStyle === Enum.VisualizerStyles.Wave
        ? Math.max(1, s.barGap)
        : s.barGap
    readonly property real canvasWidth: {
        if (s.visualizerStyle === Enum.VisualizerStyles.Wave && s.waveMode === Enum.WaveMode.Square) {
            return barCount * spacing;
        }
        if (s.visualizerStyle === Enum.VisualizerStyles.Wave
            && (s.waveMode === Enum.WaveMode.Curve || s.waveMode === Enum.WaveMode.Triangle)) {
            return (barCount - 1) * spacing;
        }
        return barCount * s.barWidth + ((barCount - 1) * spacing);
    }

    Rectangle {
        id: kirigamiColorItem
        opacity: 0
        height: 1
        width: height
        Kirigami.Theme.colorSet: Kirigami.Theme[root.s.barColorsCfg ? root.s.barColorsCfg.systemColorSet : "Window"]
    }

    Rectangle {
        id: kirigamiColorItem2
        opacity: 0
        height: 1
        width: height
        Kirigami.Theme.colorSet: Kirigami.Theme[root.s.waveFillColorsCfg ? root.s.waveFillColorsCfg.systemColorSet : "Window"]
    }

    Rectangle {
        id: kirigamiColorItem3
        opacity: 0
        height: 1
        width: height
        Kirigami.Theme.colorSet: Kirigami.Theme[root.s.inactiveBlockColorsCfg ? root.s.inactiveBlockColorsCfg.systemColorSet : "Window"]
    }

    readonly property list<color> barColors: {
        if (!s.barColorsCfg) {
            return [];
        }
        return Utils.getColors(s.barColorsCfg,
                               barCount,
                               kirigamiColorItem.Kirigami.Theme[s.barColorsCfg.systemColor]);
    }

    readonly property list<color> waveFillColors: {
        if (!s.waveFillColorsCfg) {
            return [];
        }
        return Utils.getColors(s.waveFillColorsCfg,
                               barCount,
                               kirigamiColorItem2.Kirigami.Theme[s.waveFillColorsCfg.systemColor]);
    }

    readonly property list<color> inactiveColors: {
        if (!s.inactiveBlockColorsCfg) {
            return [];
        }
        return Utils.getColors(s.inactiveBlockColorsCfg,
                               barCount,
                               kirigamiColorItem3.Kirigami.Theme[s.inactiveBlockColorsCfg.systemColor]);
    }

    SpectrumItem {
        width: root.canvasWidth
        height: root.height
        anchors.centerIn: parent

        values: root.s.values ?? []
        gradientColors: root.barColors
        waveFillColors: root.waveFillColors
        inactiveColors: root.inactiveColors
        smoothGradient: root.s.barColorsCfg ? root.s.barColorsCfg.smoothGradient : true
        waveFillSmoothGradient: root.s.waveFillColorsCfg ? root.s.waveFillColorsCfg.smoothGradient : true
        inactiveSmoothGradient: root.s.inactiveBlockColorsCfg ? root.s.inactiveBlockColorsCfg.smoothGradient : true
        gradientOrientation: root.s.barColorsCfg ? root.s.barColorsCfg.colorsOrientation : 1
        waveFillOrientation: root.s.waveFillColorsCfg ? root.s.waveFillColorsCfg.colorsOrientation : 1
        inactiveOrientation: root.s.inactiveBlockColorsCfg ? root.s.inactiveBlockColorsCfg.colorsOrientation : 1
        style: root.s.visualizerStyle
        waveMode: root.s.waveMode
        centered: root.s.centeredBars
        rounded: root.s.roundedBars
        circleMode: root.s.circleMode
        fillWave: root.s.fillWave
        drawInactive: root.s.drawInactiveBlocks
        waveform: root.s.waveform
        waveSimulateWaveform: root.s.waveSimulateWaveform
        circleModeFill: root.s.circleModeFill
        circleModeSize: root.s.circleModeSize
        barWidth: root.s.barWidth
        spacing: root.spacing
        blockHeight: root.s.blockHeight
        blockSpacing: root.s.blockSpacing
        sampleCount: 1
        alphaBlending: true
    }
}
