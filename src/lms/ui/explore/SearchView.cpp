/*
 * Copyright (C) 2020 Emeric Poupon
 *
 * This file is part of LMS.
 *
 * LMS is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * LMS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with LMS.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "SearchView.hpp"

#include <functional>

#include <Wt/WAnchor.h>
#include <Wt/WImage.h>
#include <Wt/WStackedWidget.h>

#include "database/Artist.hpp"
#include "database/Release.hpp"
#include "database/Session.hpp"
#include "database/Track.hpp"

#include "common/InfiniteScrollingContainer.hpp"
#include "common/LoadingIndicator.hpp"
#include "ArtistListHelpers.hpp"
#include "Filters.hpp"
#include "LmsApplication.hpp"
#include "ReleaseListHelpers.hpp"
#include "TrackListHelpers.hpp"

static constexpr std::size_t maxEntries {6};

namespace UserInterface
{

	SearchView::SearchView(Filters* filters)
	: Wt::WTemplate {Wt::WString::tr("Lms.Explore.Search.template")}
	, _filters {filters}
	, _releaseCollector {*filters, ReleaseCollector::Mode::Search}
	{
		addFunction("tr", &Wt::WTemplate::Functions::tr);

		Wt::WStackedWidget* stack {bindNew<Wt::WStackedWidget>("stack")};
		_menu = bindNew<Wt::WMenu>("mode", stack);

		auto addItem = [=](const Wt::WString& str, Mode mode, const Wt::WString& templateStr, std::function<void()> onRequestElementsFunc)
		{
			assert(modeToIndex(mode) == _results.size());

			auto results {std::make_unique<InfiniteScrollingContainer>(templateStr)};
			results->onRequestElements.connect(std::move(onRequestElementsFunc));

			_results.push_back(results.get());
			_menu->addItem(str, std::move(results));
		};

		// same order as Mode!
		addItem(Wt::WString::tr("Lms.Explore.releases"), Mode::Release, Wt::WString::tr("Lms.Explore.Releases.template.container"), [this]{ addSomeReleases(); });
		addItem(Wt::WString::tr("Lms.Explore.artists"), Mode::Artist, "", []{});
		addItem(Wt::WString::tr("Lms.Explore.tracks"), Mode::Track, "", []{});

		// TODO move?
		_releaseCollector.setMaxCount(ReleaseCollector::Mode::Search, getBatchSize(Mode::Release) * 30);

		_filters->updated().connect([=]
		{
			refreshView();
		});

		refreshView();
	}

	std::size_t
	SearchView::modeToIndex(Mode mode) const
	{
		return static_cast<std::size_t>(mode);
	}

	Wt::WMenuItem&
	SearchView::getItemMenu(Mode mode) const
	{
		return *_menu->itemAt(modeToIndex(mode));
	}

	InfiniteScrollingContainer&
	SearchView::getResultContainer(Mode mode) const
	{
		return *_results[modeToIndex(mode)];
	}

	std::size_t
	SearchView::getBatchSize(Mode mode) const
	{
		auto it {_batchSizes.find(mode)};
		assert(it != _batchSizes.cend());
		return it->second;
	}

	void
	SearchView::refreshView(const Wt::WString& searchText)
	{
		_searchValue = searchText.toUTF8();
		_keywords = StringUtils::splitString(_searchValue, " ");
		refreshView();
	}

	void
	SearchView::refreshView()
	{
		for (InfiniteScrollingContainer* results : _results)
			results->clear();

		addSomeReleases();

/*
		std::size_t currentIndex{};
		if (std::unique_ptr<Wt::WContainerWidget> artists {createArtistResults()})
		{
			stack->insertWidget(currentIndex, std::move(artists));
			menu->addItem(Wt::WString::tr("Lms.Explore.artists"));
			// TODO clicked
			currentIndex++;
		}

		if (std::unique_ptr<Wt::WContainerWidget> releases {createReleaseResults()})
		{
			stack->insertWidget(currentIndex, std::move(releases));
			menu->addItem(Wt::WString::tr("Lms.Explore.releases"));
			// TODO clicked
			currentIndex++;
		}

		if (std::unique_ptr<Wt::WContainerWidget> tracks {createTrackResults()})
		{
			stack->insertWidget(currentIndex, std::move(tracks));
			menu->addItem(Wt::WString::tr("Lms.Explore.tracks"));
			// TODO clicked
			currentIndex++;
		}
		*/
	}

	std::unique_ptr<Wt::WContainerWidget>
	SearchView::createArtistResults()
	{
		std::unique_ptr<Wt::WContainerWidget> container;

		bool more;
		const auto artists {Database::Artist::getByFilter(LmsApp->getDbSession(),
								_filters->getClusterIds(),
								_keywords,
								std::nullopt,
								Database::Artist::SortMethod::BySortName,
								Database::Range {0, maxEntries}, more)};

		if (!artists.empty())
		{
			container = std::make_unique<Wt::WContainerWidget>();

			for (const Database::Artist::pointer& artist : artists)
				container->addWidget(ArtistListHelpers::createEntrySmall(artist));
		}

		return container;
	}

	void
	SearchView::addSomeReleases()
	{
		InfiniteScrollingContainer& results {getResultContainer(Mode::Release)};
		bool moreResults {};

		{
			auto transaction {LmsApp->getDbSession().createSharedTransaction()};

			const Database::Range range {results.getCount(), getBatchSize(Mode::Release)};

			const auto releases {_releaseCollector.get(range, _keywords, moreResults)};
			for (const auto& release : releases)
				results.add(ReleaseListHelpers::createEntry(release));
		}

		results.setHasMore(moreResults);

		getItemMenu(Mode::Release).setDisabled(results.getCount() == 0);
	}

	std::unique_ptr<Wt::WContainerWidget>
	SearchView::createTrackResults()
	{
		std::unique_ptr<Wt::WContainerWidget> container;

		bool more;
		const auto tracks {Database::Track::getByFilter(LmsApp->getDbSession(),
								_filters->getClusterIds(),
								_keywords,
								Database::Range {0, maxEntries}, more)};

		if (!tracks.empty())
		{
			container = std::make_unique<Wt::WContainerWidget>();

			for (const Database::Track::pointer& track : tracks)
				container->addWidget(TrackListHelpers::createEntry(track, tracksAction));
		}

		return container;
	}


} // namespace UserInterface

