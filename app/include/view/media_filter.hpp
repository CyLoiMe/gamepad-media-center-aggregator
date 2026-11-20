//
// Copyright 2023 dragonflylee
//

#pragma once

#include <borealis.hpp>

class MediaFilter : public brls::Box {
public:
    MediaFilter();
    ~MediaFilter() override;

    bool isTranslucent() override { return true; }

    brls::VoidEvent* getEvent() { return &this->event; }

    /// Process-wide selection, not per library: whoever reads it next decides
    /// what the grid is sorted by. A section with no stored preference must put it
    /// back to the defaults instead of inheriting the last library's choice —
    /// `resetDefaults()` is that.
    static constexpr int kDefaultSort = 1;
    static constexpr int kDefaultOrder = 1;
    inline static int selectedSort = kDefaultSort;
    inline static int selectedOrder = kDefaultOrder;
    inline static bool selectedUnplayed = false;

    static void resetDefaults() {
        selectedSort = kDefaultSort;
        selectedOrder = kDefaultOrder;
        selectedUnplayed = false;
    }

    /// Plex sort fields, aligned with the selector labels;
    /// descending order = ":desc" suffix
    inline static std::string sortList[] = {
        "titleSort",
        "addedAt",
        "lastViewedAt",
        "originallyAvailableAt",
        "viewCount",
        "rating",
        "random",
    };

private:
    BRLS_BIND(brls::Box, cancel, "filter/cancel");
    BRLS_BIND(brls::SelectorCell, sortBy, "media/sort/by");
    BRLS_BIND(brls::SelectorCell, sortOrder, "media/sort/order");
    BRLS_BIND(brls::BooleanCell, filterUnplayed, "media/filter/unplayed");

    brls::VoidEvent event;
};