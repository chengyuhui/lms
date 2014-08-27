
#include <Wt/Http/Request>
#include <Wt/Http/Response>

#include "logger/Logger.hpp"

#include "AvConvTranscodeStreamResource.hpp"

namespace UserInterface {

AvConvTranscodeStreamResource::AvConvTranscodeStreamResource(const Transcode::Parameters& parameters, Wt::WObject *parent)
: Wt::WResource(parent),
	_parameters( parameters )
{
	LMS_LOG(MOD_UI, SEV_DEBUG) << "CONSTRUCTING RESOURCE";
}

AvConvTranscodeStreamResource::~AvConvTranscodeStreamResource()
{
	LMS_LOG(MOD_UI, SEV_DEBUG) << "DESTRUCTING RESOURCE";
	beingDeleted();
}

void
AvConvTranscodeStreamResource::handleRequest(const Wt::Http::Request& request,
		Wt::Http::Response& response)
{
	// see if this request is for a continuation:
	Wt::Http::ResponseContinuation *continuation = request.continuation();

	std::shared_ptr<Transcode::AvConvTranscoder> transcoder;
	if (continuation)
		transcoder = boost::any_cast<std::shared_ptr<Transcode::AvConvTranscoder> >(continuation->data());

	if (!transcoder)
	{
		LMS_LOG(MOD_UI, SEV_DEBUG) << "Launching transcoder";
		transcoder = std::make_shared<Transcode::AvConvTranscoder>( _parameters);

		response.setMimeType(_parameters.getOutputFormat().getMimeType());
	}

	Transcode::AvConvTranscoder::data_type& data = transcoder->getOutputData();

	while (!transcoder->isComplete() && data.size() < _bufferSize)
		transcoder->process();

	// Give the client all the output data
	Transcode::AvConvTranscoder::data_type::const_iterator it = data.begin();
	bool copySuccess = true;
	std::size_t copiedSize = 0;
	for (Transcode::AvConvTranscoder::data_type::const_iterator it = data.begin(); it != data.end(); ++it)
	{
		if (!response.out().put(*it)) {
			copySuccess = false;
			break;
		}
		else
			copiedSize++;
	}

	LMS_LOG(MOD_UI, SEV_DEBUG) << "Wrote " << copiedSize << " bytes";

	if (!copySuccess)
		LMS_LOG(MOD_UI, SEV_ERROR) << "** Write failed!";

	// Consume copied bytes
	data.erase(data.begin(), data.begin() + copiedSize);

	if (copySuccess && !transcoder->isComplete()) {
		continuation = response.createContinuation();
		continuation->setData(transcoder);
		LMS_LOG(MOD_UI, SEV_DEBUG) << "Continuation set!";
	}
	else
		LMS_LOG(MOD_UI, SEV_DEBUG) << "No more data!";
}

} // namespace UserInterface


