/**
 * @file waveform_widget.cpp
 * @brief Implementation of custom audio waveform painting and playhead tracking.
 * @author Volsa2 Project Team
 * @date 2026
 */

#include "waveform_widget.hpp"

#include <QPainter>
#include <QPaintEvent>
#include <QMouseEvent>
#include <cmath>
#include <algorithm>

WaveformWidget::WaveformWidget(QWidget* parent)
    : QWidget(parent) {
    setMinimumHeight(80);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
}

void WaveformWidget::setAudioData(const std::vector<int16_t>& samples, uint32_t sample_rate) {
    samples_ = samples;
    sample_rate_ = sample_rate;
    playhead_pos_ = -1.0;
    update();
}

void WaveformWidget::clear() {
    samples_.clear();
    playhead_pos_ = -1.0;
    update();
}

void WaveformWidget::setPlayheadPosition(double normalized_pos) {
    playhead_pos_ = std::clamp(normalized_pos, 0.0, 1.0);
    update();
}

void WaveformWidget::mousePressEvent(QMouseEvent* event) {
    if (samples_.empty() || width() <= 0) return;
    double pos = static_cast<double>(event->position().x()) / width();
    pos = std::clamp(pos, 0.0, 1.0);
    setPlayheadPosition(pos);
    emit seekRequested(pos);
}

/**
 * @brief Paints the audio envelope using vertical min/max lines per pixel.
 * @details For each horizontal pixel column x:
 *          1. Map x to the corresponding range of sample indices [start_idx, end_idx).
 *          2. Compute the minimum and maximum sample values in that interval.
 *          3. Scale min and max to the widget vertical height centered at mid_y.
 *          4. Draw a single vertical line from min to max in Volca Orange (#F07828).
 */
void WaveformWidget::paintEvent(QPaintEvent* /*event*/) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);

    // Dark graphite background
    p.fillRect(rect(), QColor(26, 26, 30));

    int w = width();
    int h = height();
    int mid_y = h / 2;

    // Center baseline
    p.setPen(QColor(50, 50, 60));
    p.drawLine(0, mid_y, w, mid_y);

    if (samples_.empty()) {
        p.setPen(QColor(100, 100, 120));
        p.drawText(rect(), Qt::AlignCenter, "No sample loaded");
        return;
    }

    // Render waveform envelope in Volca Orange
    p.setPen(QColor(240, 120, 40));

    size_t total_samples = samples_.size();
    double samples_per_pixel = static_cast<double>(total_samples) / w;

    for (int x = 0; x < w; ++x) {
        size_t start_idx = static_cast<size_t>(x * samples_per_pixel);
        size_t end_idx = static_cast<size_t>((x + 1) * samples_per_pixel);
        end_idx = std::min(end_idx, total_samples);

        if (start_idx >= end_idx) {
            start_idx = std::min(start_idx, total_samples - 1);
            end_idx = start_idx + 1;
        }

        int16_t min_s = 0;
        int16_t max_s = 0;
        for (size_t s = start_idx; s < end_idx; ++s) {
            int16_t val = samples_[s];
            if (val < min_s) min_s = val;
            if (val > max_s) max_s = val;
        }

        int y_top = mid_y - static_cast<int>((static_cast<double>(max_s) / 32768.0) * (mid_y - 2));
        int y_bot = mid_y - static_cast<int>((static_cast<double>(min_s) / 32768.0) * (mid_y - 2));

        if (y_top == y_bot) {
            y_top = mid_y - 1;
            y_bot = mid_y + 1;
        }

        p.drawLine(x, y_top, x, y_bot);
    }

    // Border framing
    p.setPen(QColor(60, 60, 75));
    p.drawRect(0, 0, w - 1, h - 1);

    // Real-time animated playhead
    if (playhead_pos_ >= 0.0 && playhead_pos_ <= 1.0) {
        int play_x = static_cast<int>(playhead_pos_ * w);
        p.setPen(QPen(QColor(255, 230, 80), 2));
        p.drawLine(play_x, 0, play_x, h);
    }

    // Text overlay: sample statistics and duration
    double dur_secs = static_cast<double>(total_samples) / sample_rate_;
    QString info_str = QString("%1 samples (%2s @ %3 Hz)")
                           .arg(total_samples)
                           .arg(QString::number(dur_secs, 'f', 2))
                           .arg(sample_rate_);

    p.setPen(QColor(220, 220, 220, 200));
    p.drawText(QRect(8, 4, w - 16, 20), Qt::AlignLeft | Qt::AlignTop, info_str);
}
