/**
 * @file waveform_widget.hpp
 * @brief Custom QWidget for hardware-style audio waveform visualization and interactive seeking.
 * @details Renders peak-min/max audio envelopes on a dark graphite background with
 *          KORG-style orange accents, supports click-to-seek, and tracks a dynamic playhead.
 * @author Volsa2 Project Team
 * @date 2026
 */

#pragma once

#include <QWidget>
#include <vector>
#include <cstdint>

/**
 * @class WaveformWidget
 * @brief Custom Qt Widget displaying audio waveforms with interactive seeking and playhead tracking.
 */
class WaveformWidget : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Constructs a WaveformWidget with default sizing and palette.
     * @param parent Optional parent QWidget.
     */
    explicit WaveformWidget(QWidget* parent = nullptr);

    /**
     * @brief Populates the widget with audio PCM sample data for rendering.
     * @param samples Contiguous vector of 16-bit PCM samples.
     * @param sample_rate Audio sampling rate in Hz (defaults to 31,250 Hz).
     */
    void setAudioData(const std::vector<int16_t>& samples, uint32_t sample_rate = 31250);

    /**
     * @brief Clears the active waveform and resets playhead indicators.
     */
    void clear();

    /**
     * @brief Updates the normalized playhead position.
     * @param normalized_pos Position between 0.0 (start) and 1.0 (end). Set to < 0 to hide.
     */
    void setPlayheadPosition(double normalized_pos);

signals:
    /**
     * @brief Emitted when the user clicks or drags within the waveform canvas.
     * @param normalized_pos Click position normalized between 0.0 and 1.0.
     */
    void seekRequested(double normalized_pos);

protected:
    /**
     * @brief Paints the background grid, center baseline, waveform envelope bars, and playhead cursor.
     * @param event QPaintEvent descriptor.
     */
    void paintEvent(QPaintEvent* event) override;

    /**
     * @brief Intercepts mouse click events to calculate seek position and emit seekRequested().
     * @param event QMouseEvent descriptor.
     */
    void mousePressEvent(QMouseEvent* event) override;

private:
    std::vector<int16_t> samples_; ///< Cached PCM audio samples.
    uint32_t sample_rate_{31250};  ///< Sample rate used for duration calculation.
    double playhead_pos_{-1.0};    ///< Normalized playhead position (-1.0 if inactive).
};
