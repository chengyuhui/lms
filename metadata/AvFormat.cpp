#include "AvFormat.hpp"

#include <boost/foreach.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include "av/InputFormatContext.hpp"

#include "Utils.hpp"

namespace MetaData
{

void
AvFormat::parse(const boost::filesystem::path& p, Items& items)
{

	try {

		Av::InputFormatContext input(p);
		input.findStreamInfo(); // needed by input.getDurationSecs

		std::map<std::string, std::string> metadata;
		input.getMetadata().get(metadata);

		// HACK or OGG files
		// If we did not find tags, searched metadata in streams
		if (metadata.empty())
		{
			// Get input streams
			std::vector<Av::Stream> streams = input.getStreams();

			BOOST_FOREACH(Av::Stream& stream, streams)
			{
				stream.getMetadata().get(metadata);

				if (!metadata.empty())
					break;
			}
		}


		// Stream info
		{
			std::vector<Av::Stream> avStreams = input.getStreams();

			std::vector<AudioStream>	audioStreams;
			std::vector<VideoStream>	videoStreams;
			std::vector<SubtitleStream>	subtitleStreams;

			BOOST_FOREACH(Av::Stream& avStream, avStreams)
			{
				switch(avStream.getCodecContext().getType())
				{
					case AVMEDIA_TYPE_VIDEO:
						if (!avStream.hasAttachedPic())
						{
							VideoStream stream;
							stream.bitRate = avStream.getCodecContext().getBitRate();
							videoStreams.push_back(stream);
						}
						break;

					case AVMEDIA_TYPE_AUDIO:
						{
							AudioStream stream;
							stream.nbChannels = avStream.getCodecContext().getNbChannels();
							stream.bitRate = avStream.getCodecContext().getBitRate();
							audioStreams.push_back(stream);
						}
						break;

					case AVMEDIA_TYPE_SUBTITLE:
						{
							subtitleStreams.push_back( SubtitleStream() );
						}
						break;

					default:
						break;
				}
			}

			if (!videoStreams.empty())
				items.insert( std::make_pair(MetaData::VideoStreams, videoStreams));
			if (!audioStreams.empty())
				items.insert( std::make_pair(MetaData::AudioStreams, audioStreams));
			if (!subtitleStreams.empty())
				items.insert( std::make_pair(MetaData::SubtitleStreams, SubtitleStreams));

		}

		// Duration
		items.insert( std::make_pair(MetaData::Duration, boost::posix_time::time_duration( boost::posix_time::seconds( input.getDurationSecs() )) ));

		// Embedded MetaData
		// Make sure to convert strings into UTF-8
		std::map<std::string, std::string>::const_iterator it;
		for (it = metadata.begin(); it != metadata.end(); ++it)
		{
			if (boost::iequals(it->first, "artist"))
				items.insert( std::make_pair(MetaData::Artist, string_to_utf8(it->second)) );
			else if (boost::iequals(it->first, "album"))
				items.insert( std::make_pair(MetaData::Album, string_to_utf8(it->second) ));
			else if (boost::iequals(it->first, "title"))
				items.insert( std::make_pair(MetaData::Title, string_to_utf8(it->second) ));
			else if (boost::iequals(it->first, "track")) {
				std::size_t number;
				if (readAs<std::size_t>(it->second, number))
					items.insert( std::make_pair(MetaData::TrackNumber, number ));
			}
			else if (boost::iequals(it->first, "disc"))
			{
				std::size_t number;
				if (readAs<std::size_t>(it->second, number))
					items.insert( std::make_pair(MetaData::DiscNumber, number ));
			}
			else if (boost::iequals(it->first, "date")
				|| boost::iequals(it->first, "year")
				|| boost::iequals(it->first, "WM/Year")
				|| boost::iequals(it->first, "TDOR")	// Original date fallback
				|| boost::iequals(it->first, "TORY")	// Original date fallback
				)
			{
				boost::posix_time::ptime p;
				if (readAsPosixTime(it->second, p))
					items.insert( std::make_pair(MetaData::CreationTime, p));
			}
			else if (boost::iequals(it->first, "genre"))
			{
				std::list<std::string> genres;
				if (readList(it->second, ";,", genres))
					items.insert( std::make_pair(MetaData::Genres, genres));

			}
/*			else
				std::cout << "key = " << it->first << ", value = " << it->second << std::endl;
*/
		}

	}
	catch(std::exception &e)
	{
		std::cerr << "Parsing of '" << p << "' failed!" << std::endl;
	}


}

} // namespace MetaData

