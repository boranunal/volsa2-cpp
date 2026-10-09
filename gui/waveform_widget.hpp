/**
 * @file waveform_widget.hpp
 * @brief Custom QWidget for hardware-style audio waveform visualization, region selection,
 *        sample chopping, beat slice markers, and interactive playhead tracking.
 * @details Renders peak-min/max audio envelopes on a dark graphite background with
 *          KORG-style orange accents, supports draggable start/end crop markers,
 *          slice boundary indicators, click-to-seek, and playhead tracking.
 * @author Volsa2 Project Team
 * @date 2026
 */

#pragma once

#include <QWidget>
#include <vector>
#include <cstdint>

/**
 * @class WaveformWidget
 * @brief Custom Qt Widget displaying audio waveforms with interactive seeking,
 *        region selection for sample chopping, and slice markers.
 */
class WaveformWidget : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Constructs a WaveformWidget.
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
     * @brief Clears the active waveform and resets playhead and selection indicators.
     */
    void clear();

    /**
     * @brief Updates the normalized playhead position.
     * @param normalized_pos Position between 0.0 (start) and 1.0 (end). Set to < 0 to hide.
     */
    void setPlayheadPosition(double normalized_pos);

    /**
     * @brief Enables or disables interactive region selection.
     * @param enabled True to allow mouse dragging of start/end crop markers.
     */
    void setSelectionEnabled(bool enabled);

    /**
     * @brief Explicitly sets the selected region in sample indices.
     * @param start_sample Starting sample index.
     * @param end_sample Ending sample index.
     */
    void setSelection(size_t start_sample, size_t end_sample);

    /**
     * @brief Clears any active selection, selecting the entire audio buffer.
     */
    void clearSelection();

    /**
     * @brief Checks if a non-default sub-region is currently selected.
     */
    [[nodiscard]] bool hasSelection() const;

    /**
     * @brief Retrieves the active selection bounds in sample indices.
     * @param out_start Populated with start sample index.
     * @param out_end Populated with end sample index.
     */
    void getSelection(size_t& out_start, size_t& out_end) const;

    /**
     * @brief Sets slice point markers for beat slicing display.
     * @param slice_sample_indices Sample indices of slice boundaries.
     */
    void setSliceMarkers(const std::vector<size_t>& slice_sample_indices);

    /**
     * @brief Clears all slice markers.
     */
    void clearSliceMarkers();

signals:
    /**
     * @brief Emitted when the user clicks to seek playback position.
     * @param normalized_pos Click position normalized between 0.0 and 1.0.
     */
    void seekRequested(double normalized_pos);

    /**
     * @brief Emitted when the crop region selection is modified.
     * @param start_sample Starting sample index.
     * @param end_sample Ending sample index.
     */
    void selectionChanged(size_t start_sample, size_t end_sample);

    /**
     * @brief Emitted when a beat slice region is clicked.
     * @param slice_index 0-based slice index.
     */
    void sliceClicked(int slice_index);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;

private:
    int sampleToPixel(size_t sample) const;
    size_t pixelToSample(int x) const;

    enum class DragMode {
        None,
        Seek,
        DragStartMarker,
        DragEndMarker,
        SelectNew
    };

    std::vector<int16_t> samples_;      ///< Cached PCM audio samples.
    uint32_t sample_rate_{31250};       ///< Sample rate used for duration calculation.
    double playhead_pos_{-1.0};         ///< Normalized playhead position (-1.0 if inactive).

    bool selection_enabled_{true};      ///< Interactive crop handle dragging enabled.
    bool has_selection_{false};         ///< True when a crop sub-region is selected.
    size_t sel_start_{0};               ///< Selection start in sample index.
    size_t sel_end_{0};                 ///< Selection end in sample index.
    DragMode drag_mode_{DragMode::None};///< Current active mouse drag interaction.
    int drag_anchor_x_{0};              ///< Anchor pixel for new selection dragging.

    std::vector<size_t> slice_markers_; ///< Slice sample boundary markers.
};
