#include <sstream>

#include <boost/iostreams/stream.hpp>
#include <boost/process.hpp>
#include <boost/foreach.hpp>

#include "logger/Logger.hpp"

#include "AvConvTranscoder.hpp"

namespace Transcode
{

boost::mutex	AvConvTranscoder::_mutex;

boost::filesystem::path	AvConvTranscoder::_avConvPath = "";

void
AvConvTranscoder::init()
{
	_avConvPath = boost::process::search_path("avconv");
	LMS_LOG(MOD_TRANSCODE, SEV_INFO) << "Using execPath " << _avConvPath;
}


//boost::filesystem::path AvConvTranscoder::_avConvPath = "";

AvConvTranscoder::AvConvTranscoder(const Parameters& parameters)
: _parameters(parameters),
  _outputPipe(boost::process::create_pipe()),
  _source(_outputPipe.source, boost::iostreams::close_handle),
  _is(_source),
  _in(&_is),
  _isComplete(false)
{

	if (!boost::filesystem::exists(_parameters.getInputMediaFile().getPath())) {
		throw std::runtime_error("File " +  _parameters.getInputMediaFile().getPath().string() + " does not exists!");
	}
	else if (!boost::filesystem::is_regular( _parameters.getInputMediaFile().getPath())) {
		throw std::runtime_error("File " +  _parameters.getInputMediaFile().getPath().string() + " is not regular!");
	}

	LMS_LOG(MOD_TRANSCODE, SEV_INFO) << "Transcoding file '" << _parameters.getInputMediaFile().getPath() << "'";

	// Launch a process to handle the conversion
	boost::iostreams::file_descriptor_sink sink(_outputPipe.sink, boost::iostreams::close_handle);

	std::ostringstream oss;

	oss << "avconv";

	// input Offset
	if (_parameters.getOffset().total_seconds() > 0)
		oss << " -ss " << _parameters.getOffset().total_seconds();		// to be placed before '-i' to speed up seeking

	// Input file
	oss << " -i \"" << _parameters.getInputMediaFile().getPath().string() << "\"";

	// Output bitrates
	oss << " -b:a " << _parameters.getOutputAudioBitrate() ;
	if (_parameters.getOutputFormat().getType() == Format::Video)
		oss << " -b:v " << _parameters.getOutputVideoBitrate();

	// Stream mapping
	{
		typedef std::map<Stream::Type, Stream::Id> StreamMap;

		const StreamMap& streamMap = _parameters.getInputStreams();

		BOOST_FOREACH(const StreamMap::value_type& inputStream, streamMap)
		{
			// HACK (no subtitle support yet)
			if (inputStream.first != Stream::Subtitle)
				oss << " -map 0:" << inputStream.second;	// 0 means input file index
		}
	}

	// Codecs and formats
	switch( _parameters.getOutputFormat().getEncoding())
	{
		case Format::MP3:
			oss << " -f mp3";
			break;
		case Format::OGA:
			oss << " -acodec libvorbis -f ogg";
			break;
		case Format::OGV:
			oss << " -acodec libvorbis -ac 2 -ar 44100 -vcodec libtheora -threads 4 -f ogg";
			break;
		case Format::WEBMA:
			oss << " -codec:a libvorbis -f webm";
			break;
		case Format::WEBMV:
			oss << " -acodec libvorbis -ac 2 -ar 44100 -vcodec libvpx -threads 4 -f webm";
			break;
		case Format::M4A:
			oss << " -acodec aac -f mp4";
			break;
		case Format::M4V:
			oss << " -acodec aac -strict experimental -ac 2 -ar 44100 -vcodec libx264 -f m4v";
			break;
		case Format::FLV:
			oss << " -acodec libmp3lame -ac 2 -ar 44100 -vcodec libx264 -f flv";
			break;
		default:
			assert(0);
	}
	oss << " -";		// output to stdout

	LMS_LOG(MOD_TRANSCODE, SEV_DEBUG) << "Executing '" << oss.str() << "'";

	// make sure only one thread is executing this part of code
	// See boost process FAQ
	{
		boost::lock_guard<boost::mutex>	lock(_mutex);
		std::vector<int> ranges = { 3, 1024 };	// fd range to be closed

		_child = std::make_shared<boost::process::child>( boost::process::execute(
					boost::process::initializers::run_exe(_avConvPath),
					boost::process::initializers::set_cmd_line(oss.str()),
					boost::process::initializers::bind_stdout(sink),
					boost::process::initializers::close_fd(STDIN_FILENO),
					boost::process::initializers::close_fds(ranges)
					)
				);
	}

}

void
AvConvTranscoder::process(void)
{
	std::size_t readDatasSize = 1024; // TODO parametrize elsewhere?

	char ch;
	while(readDatasSize != 0 && _in && _in.get(ch)) {
		_data.push_back(ch);
		--readDatasSize;
	}

	if (!_in || _in.fail() || _in.eof()) {
		LMS_LOG(MOD_TRANSCODE, SEV_DEBUG) << "Transcode complete!";

		waitChild();
		_isComplete = true;
	}
}


AvConvTranscoder::~AvConvTranscoder()
{
	LMS_LOG(MOD_TRANSCODE, SEV_DEBUG) << "~AvConvTranscoder called!";

	if (_in.eof())
		waitChild();
	else
		killChild();
}

void
AvConvTranscoder::waitChild()
{
	if (_child)
	{
		boost::system::error_code ec;

		LMS_LOG(MOD_TRANSCODE, SEV_DEBUG) << "Waiting for child...";
		boost::process::wait_for_exit(*_child, ec);
		LMS_LOG(MOD_TRANSCODE, SEV_DEBUG) << "Waiting for child: OK";

		if (ec)
			LMS_LOG(MOD_TRANSCODE, SEV_ERROR) << "AvConvTranscoder::waitChild: error: " << ec.message();

		_child.reset();
	}
}

void
AvConvTranscoder::killChild()
{
	if (_child)
	{
		boost::system::error_code ec;

		LMS_LOG(MOD_TRANSCODE, SEV_DEBUG) << "Killing child! pid = " << _child->pid;
		boost::process::terminate(*_child, ec);
		LMS_LOG(MOD_TRANSCODE, SEV_DEBUG) << "Killing child DONE";

		// If an error occured, force kill the child
		if (ec)
			LMS_LOG(MOD_TRANSCODE, SEV_ERROR) << "AvConvTranscoder::killChild: error: " << ec.message();

		_child.reset();
	}
}

} // namespace Transcode
