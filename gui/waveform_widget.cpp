/**
 * @file waveform_widget.cpp
 * @brief Implementation of custom audio waveform painting, interactive region selection,
 *        beat slicing indicators, and playhead tracking.
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
    setMouseTracking(true);
}

void WaveformWidget::setAudioData(const std::vector<int16_t>& samples, uint32_t sample_rate) {
    samples_ = samples;
    sample_rate_ = sample_rate;
    playhead_pos_ = -1.0;
    sel_start_ = 0;
    sel_end_ = samples_.size();
    has_selection_ = false;
    slice_markers_.clear();
    update();
}

void WaveformWidget::clear() {
    samples_.clear();
    playhead_pos_ = -1.0;
    sel_start_ = 0;
    sel_end_ = 0;
    has_selection_ = false;
    slice_markers_.clear();
    update();
}

void WaveformWidget::setPlayheadPosition(double normalized_pos) {
    if (normalized_pos < 0.0) {
        playhead_pos_ = -1.0;
    } else {
        playhead_pos_ = std::clamp(normalized_pos, 0.0, 1.0);
    }
    update();
}

void WaveformWidget::setSelectionEnabled(bool enabled) {
    selection_enabled_ = enabled;
    update();
}

void WaveformWidget::setSelection(size_t start_sample, size_t end_sample) {
    if (samples_.empty()) return;
    size_t s = std::min(start_sample, samples_.size());
    size_t e = std::min(end_sample, samples_.size());
    if (s > e) std::swap(s, e);
    bool new_has_sel = (s < e);
    if (s == sel_start_ && e == sel_end_ && has_selection_ == new_has_sel) {
        return;
    }
    sel_start_ = s;
    sel_end_ = e;
    has_selection_ = new_has_sel;
    update();
    emit selectionChanged(sel_start_, sel_end_);
}

void WaveformWidget::clearSelection() {
    if (samples_.empty()) return;
    sel_start_ = 0;
    sel_end_ = samples_.size();
    has_selection_ = false;
    update();
    emit selectionChanged(sel_start_, sel_end_);
}

bool WaveformWidget::hasSelection() const {
    return has_selection_;
}

void WaveformWidget::getSelection(size_t& out_start, size_t& out_end) const {
    out_start = sel_start_;
    out_end = sel_end_;
}

void WaveformWidget::setSliceSpans(const std::vector<SliceSpan>& spans) {
    slice_spans_ = spans;
    slice_markers_.clear();
    for (const auto& span : spans) {
        slice_markers_.push_back(span.start);
    }
    if (!spans.empty()) {
        slice_markers_.push_back(spans.back().end);
    }
    update();
}

void WaveformWidget::setSliceMarkers(const std::vector<size_t>& slice_sample_indices) {
    slice_markers_ = slice_sample_indices;
    slice_spans_.clear();
    for (size_t i = 0; i + 1 < slice_sample_indices.size(); ++i) {
        slice_spans_.push_back({slice_sample_indices[i], slice_sample_indices[i + 1]});
    }
    update();
}

void WaveformWidget::clearSliceMarkers() {
    slice_spans_.clear();
    slice_markers_.clear();
    update();
}

int WaveformWidget::sampleToPixel(size_t sample) const {
    if (samples_.empty() || width() <= 0) return 0;
    return static_cast<int>((static_cast<double>(sample) / samples_.size()) * width());
}

size_t WaveformWidget::pixelToSample(int x) const {
    if (samples_.empty() || width() <= 0) return 0;
    double ratio = static_cast<double>(x) / width();
    ratio = std::clamp(ratio, 0.0, 1.0);
    return static_cast<size_t>(ratio * samples_.size());
}

void WaveformWidget::mousePressEvent(QMouseEvent* event) {
    if (samples_.empty() || width() <= 0) return;

    int x = static_cast<int>(event->position().x());
    size_t clicked_sample = pixelToSample(x);

    // 1. If selection handles exist, check if user clicked on start/end marker to drag
    if (selection_enabled_ && has_selection_) {
        int x_start = sampleToPixel(sel_start_);
        int x_end = sampleToPixel(sel_end_);

        if (std::abs(x - x_start) <= 6) {
            drag_mode_ = DragMode::DragStartMarker;
            return;
        }
        if (std::abs(x - x_end) <= 6) {
            drag_mode_ = DragMode::DragEndMarker;
            return;
        }
    }

    // 2. Check if user clicked near an interior slice divider line to drag it directly!
    if (!slice_spans_.empty()) {
        for (size_t s = 1; s < slice_spans_.size(); ++s) {
            int div_x = sampleToPixel(slice_spans_[s].start);
            if (std::abs(x - div_x) <= 5) {
                drag_mode_ = DragMode::DragSliceDivider;
                dragged_divider_idx_ = static_cast<int>(s);
                return;
            }
        }
    } else if (!slice_markers_.empty()) {
        for (size_t s = 1; s + 1 < slice_markers_.size(); ++s) {
            int div_x = sampleToPixel(slice_markers_[s]);
            if (std::abs(x - div_x) <= 5) {
                drag_mode_ = DragMode::DragSliceDivider;
                dragged_divider_idx_ = static_cast<int>(s);
                return;
            }
        }
    }

    // 3. If slice spans/markers exist, check if a slice was clicked to select
    if (!slice_spans_.empty()) {
        for (size_t s = 0; s < slice_spans_.size(); ++s) {
            if (clicked_sample >= slice_spans_[s].start &&
                (clicked_sample < slice_spans_[s].end || (s + 1 == slice_spans_.size() && clicked_sample <= slice_spans_[s].end))) {
                drag_mode_ = DragMode::None;
                emit sliceClicked(static_cast<int>(s));
                return;
            }
        }
    } else if (!slice_markers_.empty()) {
        for (size_t s = 0; s + 1 < slice_markers_.size(); ++s) {
            if (clicked_sample >= slice_markers_[s] &&
                (clicked_sample < slice_markers_[s + 1] || (s + 2 == slice_markers_.size() && clicked_sample <= slice_markers_[s + 1]))) {
                drag_mode_ = DragMode::None;
                emit sliceClicked(static_cast<int>(s));
                return;
            }
        }
    }

    if (selection_enabled_ && (event->button() == Qt::LeftButton && (event->modifiers() & Qt::ShiftModifier))) {
        drag_mode_ = DragMode::SelectNew;
        drag_anchor_x_ = x;
        sel_start_ = clicked_sample;
        sel_end_ = clicked_sample;
        has_selection_ = true;
        update();
        return;
    }

    // Default click: seek
    drag_mode_ = DragMode::Seek;
    double norm_pos = static_cast<double>(x) / width();
    setPlayheadPosition(norm_pos);
    emit seekRequested(norm_pos);
}

void WaveformWidget::mouseMoveEvent(QMouseEvent* event) {
    int x = static_cast<int>(event->position().x());

    // Update cursor hover indicator
    if (drag_mode_ == DragMode::None) {
        bool hover_hand = false;
        if (selection_enabled_ && has_selection_) {
            int x_start = sampleToPixel(sel_start_);
            int x_end = sampleToPixel(sel_end_);
            if (std::abs(x - x_start) <= 6 || std::abs(x - x_end) <= 6) {
                setCursor(Qt::SizeHorCursor);
                hover_hand = true;
            }
        }
        if (!hover_hand) {
            if (!slice_spans_.empty()) {
                for (size_t s = 1; s < slice_spans_.size(); ++s) {
                    int div_x = sampleToPixel(slice_spans_[s].start);
                    if (std::abs(x - div_x) <= 5) {
                        setCursor(Qt::SplitHCursor);
                        hover_hand = true;
                        break;
                    }
                }
            } else if (!slice_markers_.empty()) {
                for (size_t s = 1; s + 1 < slice_markers_.size(); ++s) {
                    int div_x = sampleToPixel(slice_markers_[s]);
                    if (std::abs(x - div_x) <= 5) {
                        setCursor(Qt::SplitHCursor);
                        hover_hand = true;
                        break;
                    }
                }
            }
        }
        if (!hover_hand) {
            setCursor(Qt::ArrowCursor);
        }
    }

    if (drag_mode_ == DragMode::DragStartMarker) {
        size_t new_start = pixelToSample(x);
        new_start = std::min(new_start, sel_end_);
        sel_start_ = new_start;
        has_selection_ = true;
        update();
        emit selectionChanged(sel_start_, sel_end_);
    } else if (drag_mode_ == DragMode::DragEndMarker) {
        size_t new_end = pixelToSample(x);
        new_end = std::max(new_end, sel_start_);
        sel_end_ = new_end;
        has_selection_ = true;
        update();
        emit selectionChanged(sel_start_, sel_end_);
    } else if (drag_mode_ == DragMode::DragSliceDivider) {
        size_t new_smpl = pixelToSample(x);
        if (!slice_spans_.empty() && dragged_divider_idx_ > 0 && static_cast<size_t>(dragged_divider_idx_) < slice_spans_.size()) {
            size_t min_s = slice_spans_[dragged_divider_idx_ - 1].start + 32;
            size_t max_s = (slice_spans_[dragged_divider_idx_].end > 32) ? (slice_spans_[dragged_divider_idx_].end - 32) : slice_spans_[dragged_divider_idx_].end;
            new_smpl = std::clamp(new_smpl, min_s, max_s);
            emit sliceDividerMoved(dragged_divider_idx_, new_smpl);
        } else if (!slice_markers_.empty() && dragged_divider_idx_ > 0 && static_cast<size_t>(dragged_divider_idx_ + 1) < slice_markers_.size()) {
            size_t min_s = slice_markers_[dragged_divider_idx_ - 1] + 32;
            size_t max_s = (slice_markers_[dragged_divider_idx_ + 1] > 32) ? (slice_markers_[dragged_divider_idx_ + 1] - 32) : slice_markers_[dragged_divider_idx_ + 1];
            new_smpl = std::clamp(new_smpl, min_s, max_s);
            emit sliceDividerMoved(dragged_divider_idx_, new_smpl);
        }
    } else if (drag_mode_ == DragMode::SelectNew) {
        size_t cur_sample = pixelToSample(x);
        size_t anchor_sample = pixelToSample(drag_anchor_x_);
        sel_start_ = std::min(anchor_sample, cur_sample);
        sel_end_ = std::max(anchor_sample, cur_sample);
        has_selection_ = (sel_start_ < sel_end_);
        update();
        emit selectionChanged(sel_start_, sel_end_);
    } else if (drag_mode_ == DragMode::Seek) {
        double norm_pos = static_cast<double>(x) / width();
        norm_pos = std::clamp(norm_pos, 0.0, 1.0);
        setPlayheadPosition(norm_pos);
        emit seekRequested(norm_pos);
    }
}

void WaveformWidget::mouseReleaseEvent(QMouseEvent* /*event*/) {
    drag_mode_ = DragMode::None;
    dragged_divider_idx_ = -1;
    setCursor(Qt::ArrowCursor);
}

void WaveformWidget::mouseDoubleClickEvent(QMouseEvent* /*event*/) {
    if (selection_enabled_) {
        clearSelection();
    }
}

void WaveformWidget::paintEvent(QPaintEvent* /*event*/) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    int w = width();
    int h = height();
    if (w <= 0 || h <= 0) return;

    // 1. Dark graphite background
    p.fillRect(rect(), QColor(20, 20, 24));

    int mid_y = h / 2;

    // Center baseline
    p.setPen(QColor(42, 42, 50));
    p.drawLine(0, mid_y, w, mid_y);

    if (samples_.empty()) {
        p.setPen(QColor(110, 110, 130));
        p.drawText(rect(), Qt::AlignCenter, "No sample loaded");
        return;
    }

    size_t total_samples = samples_.size();
    double samples_per_pixel = static_cast<double>(total_samples) / w;

    // 2. Render waveform envelope bars in vibrant Volca Orange across all columns
    p.setRenderHint(QPainter::Antialiasing, false);
    p.setPen(QColor(245, 125, 30)); // Bright Korg Orange

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

    // 3. Region Masking: Clean dark scrim overlay on unselected outer regions
    int sel_x_start = std::clamp(sampleToPixel(sel_start_), 0, w);
    int sel_x_end = std::clamp(sampleToPixel(sel_end_), 0, w);
    if (sel_x_start > sel_x_end) std::swap(sel_x_start, sel_x_end);

    if (has_selection_) {
        // Active selection region: subtle amber tint
        if (sel_x_end > sel_x_start) {
            p.fillRect(sel_x_start, 0, sel_x_end - sel_x_start, h, QColor(245, 125, 30, 20));
        }

        // Dark shadow overlay over unselected region on the left [0, sel_x_start]
        if (sel_x_start > 0) {
            p.fillRect(0, 0, sel_x_start, h, QColor(10, 10, 16, 185));
        }
        // Dark shadow overlay over unselected region on the right [sel_x_end, w]
        if (sel_x_end < w) {
            p.fillRect(sel_x_end, 0, w - sel_x_end, h, QColor(10, 10, 16, 185));
        }

        // Active crop region: bright guide borders
        p.setPen(QPen(QColor(245, 125, 30, 180), 1));
        p.drawLine(sel_x_start, 0, sel_x_end, 0);
        p.drawLine(sel_x_start, h - 1, sel_x_end, h - 1);
    }

    // 4. Render slice boundary markers
    if (!slice_spans_.empty()) {
        p.setRenderHint(QPainter::Antialiasing, true);
        QFont font = p.font();
        font.setPointSize(8);
        p.setFont(font);

        for (size_t s = 0; s < slice_spans_.size(); ++s) {
            int sx = sampleToPixel(slice_spans_[s].start);
            int ex = sampleToPixel(slice_spans_[s].end);
            p.setPen(QPen(QColor(0, 210, 255, 180), 1, Qt::DashLine));
            p.drawLine(sx, 0, sx, h);
            if (s + 1 == slice_spans_.size() || slice_spans_[s].end != slice_spans_[s + 1].start) {
                p.drawLine(ex, 0, ex, h);
            }

            int center_x = (sx + ex) / 2;
            p.setPen(QColor(0, 210, 255));
            p.drawText(QRect(center_x - 12, h - 18, 24, 16), Qt::AlignCenter, QString("[%1]").arg(s + 1));
        }
    } else if (!slice_markers_.empty()) {
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setPen(QPen(QColor(0, 210, 255, 180), 1, Qt::DashLine));
        QFont font = p.font();
        font.setPointSize(8);
        p.setFont(font);

        for (size_t s = 0; s < slice_markers_.size(); ++s) {
            int sx = sampleToPixel(slice_markers_[s]);
            p.drawLine(sx, 0, sx, h);

            if (s + 1 < slice_markers_.size()) {
                int next_sx = sampleToPixel(slice_markers_[s + 1]);
                int center_x = (sx + next_sx) / 2;
                p.setPen(QColor(0, 210, 255));
                p.drawText(QRect(center_x - 12, h - 18, 24, 16), Qt::AlignCenter, QString("[%1]").arg(s + 1));
            }
        }
    }

    // 5. Render selection boundary markers and draggable handles
    if (has_selection_) {
        p.setRenderHint(QPainter::Antialiasing, true);

        // Start marker (Cyan #00E5B0)
        p.setPen(QPen(QColor(0, 229, 176), 2));
        p.drawLine(sel_x_start, 0, sel_x_start, h);
        // Start handle flag
        QPolygon start_flag;
        start_flag << QPoint(sel_x_start, 0)
                   << QPoint(sel_x_start + 14, 0)
                   << QPoint(sel_x_start + 14, 12)
                   << QPoint(sel_x_start, 16);
        p.setBrush(QColor(0, 229, 176));
        p.drawPolygon(start_flag);
        p.setBrush(Qt::NoBrush);
        p.setPen(QColor(10, 20, 20));
        QFont f = p.font();
        f.setPixelSize(9);
        f.setBold(true);
        p.setFont(f);
        p.drawText(QRect(sel_x_start + 2, 1, 11, 11), Qt::AlignCenter, "S");

        // End marker (Coral Red #FF4444)
        p.setPen(QPen(QColor(255, 68, 68), 2));
        p.drawLine(sel_x_end, 0, sel_x_end, h);
        // End handle flag
        QPolygon end_flag;
        end_flag << QPoint(sel_x_end, 0)
                 << QPoint(sel_x_end - 14, 0)
                 << QPoint(sel_x_end - 14, 12)
                 << QPoint(sel_x_end, 16);
        p.setBrush(QColor(255, 68, 68));
        p.drawPolygon(end_flag);
        p.setBrush(Qt::NoBrush);
        p.setPen(QColor(255, 255, 255));
        p.drawText(QRect(sel_x_end - 13, 1, 11, 11), Qt::AlignCenter, "E");
    }

    // 6. Border framing (explicitly ensure Qt::NoBrush so interior is not filled!)
    p.setRenderHint(QPainter::Antialiasing, false);
    p.setBrush(Qt::NoBrush);
    p.setPen(QColor(50, 50, 60));
    p.drawRect(0, 0, w - 1, h - 1);

    // 7. Real-time animated playhead
    if (playhead_pos_ >= 0.0 && playhead_pos_ <= 1.0) {
        int play_x = static_cast<int>(playhead_pos_ * w);
        p.setPen(QPen(QColor(255, 235, 70), 2));
        p.drawLine(play_x, 0, play_x, h);
    }

    // 8. Text overlay: sample statistics and selection duration
    p.setRenderHint(QPainter::Antialiasing, true);
    double dur_secs = static_cast<double>(total_samples) / sample_rate_;
    QString info_str = QString("%1 smpls (%2s @ %3 Hz)")
                           .arg(total_samples)
                           .arg(QString::number(dur_secs, 'f', 2))
                           .arg(sample_rate_);

    if (has_selection_) {
        size_t sel_len = (sel_end_ > sel_start_) ? (sel_end_ - sel_start_) : 0;
        double sel_secs = static_cast<double>(sel_len) / sample_rate_;
        info_str += QString(" | Crop: %1 smpls (%2s)")
                        .arg(sel_len)
                        .arg(QString::number(sel_secs, 'f', 2));
    }

    p.setPen(QColor(210, 210, 220, 220));
    QFont font = p.font();
    font.setPointSize(9);
    p.setFont(font);
    p.drawText(QRect(8, 4, w - 16, 20), Qt::AlignLeft | Qt::AlignTop, info_str);
}
