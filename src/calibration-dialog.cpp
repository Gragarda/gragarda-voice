// Assistant de calibration (Qt). Ouvert depuis le bouton du panneau, thread UI.
#include <obs-module.h>
#include <QDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>
#include <memory>
#include "Calibration.h"

static QString T(const char *key)
{
	return QString::fromUtf8(obs_module_text(key));
}

static QString dot(Light l)
{
	const char *c = l == Light::Green ? "#2ecc71" : l == Light::Orange ? "#f39c12" : "#e74c3c";
	return QString("<span style='color:%1; font-size:18px'>&#9679;</span>").arg(c);
}

class CalibrationDialog : public QDialog {
public:
	CalibrationDialog(obs_source_t *src, std::shared_ptr<Capture> cap, double sr)
		: weak_(obs_source_get_weak_source(src)),
		  cap_(std::move(cap)),
		  sr_(sr)
	{
		setWindowTitle(T("CalibTitle"));
		setAttribute(Qt::WA_DeleteOnClose);
		setMinimumWidth(640);
		auto *lay = new QVBoxLayout(this);
		text_ = new QLabel(T("CalibIntro"));
		text_->setWordWrap(true);
		text_->setTextFormat(Qt::RichText);
		bar_ = new QProgressBar;
		bar_->setRange(0, 100);
		bar_->hide();
		auto *buttons = new QHBoxLayout;
		cancel_ = new QPushButton(T("CalibCancel"));
		go_ = new QPushButton(T("CalibStart"));
		go_->setDefault(true);
		buttons->addStretch();
		buttons->addWidget(cancel_);
		buttons->addWidget(go_);
		lay->addWidget(text_);
		lay->addWidget(bar_);
		lay->addLayout(buttons);

		connect(cancel_, &QPushButton::clicked, this, &QDialog::close);
		connect(go_, &QPushButton::clicked, this, [this] { next(); });
		connect(&timer_, &QTimer::timeout, this, [this] { poll(); });
	}

	~CalibrationDialog() override
	{
		cap_->stop();
		obs_weak_source_release(weak_);
	}

private:
	enum Step { Intro, Noise, Speech, Result };
	Step step_ = Intro;
	obs_weak_source_t *weak_;
	std::shared_ptr<Capture> cap_;
	double sr_;
	QLabel *text_;
	QProgressBar *bar_;
	QPushButton *go_, *cancel_;
	QTimer timer_;
	int stalled_ = 0;
	float lastProgress_ = 0;
	std::vector<float> noise_;
	CalibrationReport rep_;

	void setText(const QString &html)
	{
		text_->setText(html);
		adjustSize(); // la fenetre suit la hauteur du texte (lecture, resultats)
	}

	void record(Step s, double secs, const char *textKey)
	{
		step_ = s;
		setText(T(textKey));
		bar_->setValue(0);
		bar_->show();
		go_->setEnabled(false);
		stalled_ = 0;
		lastProgress_ = 0;
		cap_->start((size_t)(secs * sr_));
		timer_.start(50);
	}

	void next()
	{
		switch (step_) {
		case Intro:
		case Result: // "Recommencer" ... ou "Appliquer" (gere dans showResult)
			record(Noise, 5.0, "CalibNoise");
			break;
		default:
			break;
		}
	}

	void poll()
	{
		const float p = cap_->progress();
		bar_->setValue((int)(p * 100));
		// Aucun son recu (micro coupe, filtre desactive, source muette) : on previent
		stalled_ = p > lastProgress_ ? 0 : stalled_ + 1;
		lastProgress_ = p;
		if (stalled_ == 30)
			setText(text_->text() + "<p><b>" + T("CalibNoAudio") + "</b></p>");
		if (!cap_->done())
			return;
		timer_.stop();
		cap_->stop();
		if (step_ == Noise) {
			noise_ = cap_->take();
			record(Speech, 15.0, "CalibSpeech");
		} else if (step_ == Speech) {
			rep_ = analyzeCalibration(noise_, cap_->take(), sr_);
			showResult();
		}
	}

	void showResult()
	{
		step_ = Result;
		bar_->hide();
		go_->setEnabled(true);
		const auto &r = rep_;
		blog(LOG_INFO,
		     "[gragarda-voice] calibration : bruit %.1f LUFS, parole %.1f LUFS, SNR %.1f, crete %.1f dBFS, sature %.3f%%, "
		     "grave %+.1f, presence %+.1f, sifflantes %+.1f (pics %.1f), dynamique %.1f -> gain %+.1f, N%d C%d Cl%d P%d, correction %+.1f/%+.1f/%+.1f dB, RT60 %.2f s -> de-reverb %.2f",
		     r.noiseLufs, r.speechLufs, r.snrDb, r.peakDb, r.clipPct, r.lowDb, r.presenceDb, r.sibilanceDb,
		     r.sibPeakDb, r.spreadDb, r.gainDb, r.clean, r.warmth, r.clarity, r.punch, r.corrLow,
		     r.corrPresence, r.corrAir, r.rt60, r.dereverbRt60);

		if (!r.valid) {
			setText(T("CalibInvalid"));
			go_->setText(T("CalibRetry"));
			return;
		}
		auto light = [](Light l, const char *name, const char *g, const char *o, const char *rd) {
			return "<tr><td>" + dot(l) + "</td><td><b>" + T(name) + "</b></td><td>" +
			       T(l == Light::Green    ? g
				 : l == Light::Orange ? o
						      : rd) +
			       "</td></tr>";
		};
		QString html = "<p>" + T("CalibDone") + "</p><table cellspacing='6'>";
		html += light(r.noise, "LightNoise", "NoiseG", "NoiseO", "NoiseR");
		html += light(r.level, "LightLevel", "LevelG", "LevelO", "LevelR");
		html += light(r.sibilance, "LightSib", "SibG", "SibO", "SibR");
		html += light(r.clipping, "LightClip", "ClipG", "ClipO", "ClipR");
		html += "<tr><td>" + dot(r.reverb) + "</td><td><b>" + T("LightReverb") + "</b></td><td>" +
			(r.rt60 <= 0 ? T("ReverbNA")
				     : T(r.dereverbRt60 > 0 ? "ReverbOn" : "ReverbOff").arg(r.rt60, 0, 'f', 2)) +
			"</td></tr>";
		html += "</table><p>" + T("CalibProposal") + "</p><table cellspacing='4'>";
		html += QString("<tr><td>%1</td><td><b>%2 dB</b></td></tr>").arg(T("Gain")).arg(r.gainDb, 0, 'f', 1);
		html += QString("<tr><td>%1</td><td><b>%2</b></td></tr>").arg(T("Clean")).arg(r.clean);
		html += QString("<tr><td>%1</td><td><b>%2</b></td></tr>").arg(T("Warmth")).arg(r.warmth);
		html += QString("<tr><td>%1</td><td><b>%2</b></td></tr>").arg(T("Clarity")).arg(r.clarity);
		html += QString("<tr><td>%1</td><td><b>%2</b></td></tr>").arg(T("Punch")).arg(r.punch);
		html += QString("<tr><td>%1</td><td><b>%2 / %3 / %4 dB</b></td></tr>")
				.arg(T("MicCorrection"))
				.arg(r.corrLow, 0, 'f', 1)
				.arg(r.corrPresence, 0, 'f', 1)
				.arg(r.corrAir, 0, 'f', 1);
		html += "</table>";
		setText(html);

		// Bouton principal = Appliquer ; Annuler devient "Recommencer"
		go_->setText(T("CalibApply"));
		go_->disconnect();
		connect(go_, &QPushButton::clicked, this, [this] { apply(); });
		cancel_->setText(T("CalibRetry"));
		cancel_->disconnect();
		connect(cancel_, &QPushButton::clicked, this, [this] { record(Noise, 5.0, "CalibNoise"); });
	}

	void apply()
	{
		if (obs_source_t *src = obs_weak_source_get_source(weak_)) {
			obs_data_t *s = obs_source_get_settings(src);
			obs_data_set_double(s, "gain_db", rep_.gainDb);
			obs_data_set_int(s, "m_clean", rep_.clean);
			obs_data_set_int(s, "m_warmth", rep_.warmth);
			obs_data_set_int(s, "m_clarity", rep_.clarity);
			obs_data_set_int(s, "m_punch", rep_.punch);
			obs_data_set_double(s, "corr_low", rep_.corrLow);
			obs_data_set_double(s, "corr_presence", rep_.corrPresence);
			obs_data_set_double(s, "corr_air", rep_.corrAir);
			obs_data_set_double(s, "dereverb_rt60", rep_.dereverbRt60);
			obs_source_update(src, s);
			obs_source_update_properties(src); // rafraichit le panneau s'il est ouvert
			obs_data_release(s);
			obs_source_release(src);
		}
		close();
	}
};

void gv_open_calibration(obs_source_t *src, std::shared_ptr<Capture> cap, double sampleRate)
{
	auto *d = new CalibrationDialog(src, std::move(cap), sampleRate);
	d->show();
	d->raise();
	d->activateWindow();
}
