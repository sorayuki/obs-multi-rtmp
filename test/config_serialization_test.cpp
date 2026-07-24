#include "doctest.h"
#include "config-serialization.h"

TEST_CASE("target round-trips including new fields") {
    MultiOutputConfig cfg;
    auto t = std::make_shared<OutputTargetConfig>();
    t->id = "t1";
    t->name = "Twitch";
    t->protocol = "RTMP";
    t->syncStart = true;
    t->autoStart = true;
    t->autoRestart = true;
    t->maxRestarts = 3;
    cfg.targets.push_back(t);

    auto json = ConfigToJsonString(cfg);
    auto back = ConfigFromJsonString(json);
    REQUIRE(back.targets.size() == 1);
    auto& r = back.targets.front();
    CHECK(r->id == "t1");
    CHECK(r->name == "Twitch");
    CHECK(r->autoStart == true);
    CHECK(r->autoRestart == true);
    CHECK(r->maxRestarts == 3);
}

TEST_CASE("legacy config without new fields uses defaults") {
    // no auto-start/auto-restart keys present
    std::string legacy = R"({"targets":[{"id":"x","name":"n","protocol":"RTMP"}]})";
    auto cfg = ConfigFromJsonString(legacy);
    REQUIRE(cfg.targets.size() == 1);
    CHECK(cfg.targets.front()->autoStart == false);
    CHECK(cfg.targets.front()->maxRestarts == 5);
}

TEST_CASE("RemapImportedIds avoids collisions") {
    MultiOutputConfig existing;
    auto e = std::make_shared<OutputTargetConfig>(); e->id = "dup";
    existing.targets.push_back(e);

    MultiOutputConfig imported;
    auto i = std::make_shared<OutputTargetConfig>(); i->id = "dup";
    imported.targets.push_back(i);

    RemapImportedIds(imported, existing);
    CHECK(imported.targets.front()->id != "dup");
}

TEST_CASE("RemapImportedIds rewrites encoder-config id collisions and fixes up references") {
    MultiOutputConfig existing;
    auto ev = std::make_shared<VideoEncoderConfig>(); ev->id = "v1";
    existing.videoConfig.push_back(ev);
    auto ea = std::make_shared<AudioEncoderConfig>(); ea->id = "a1";
    existing.audioConfig.push_back(ea);

    MultiOutputConfig imported;
    auto iv = std::make_shared<VideoEncoderConfig>(); iv->id = "v1"; // collides with existing
    imported.videoConfig.push_back(iv);
    auto ia = std::make_shared<AudioEncoderConfig>(); ia->id = "a1"; // collides with existing
    imported.audioConfig.push_back(ia);

    auto t = std::make_shared<OutputTargetConfig>();
    t->id = "target-1";
    t->videoConfig = "v1";
    t->audioConfig = "a1";
    imported.targets.push_back(t);

    RemapImportedIds(imported, existing);

    // encoder-config ids must have changed to avoid colliding with existing
    CHECK(imported.videoConfig.front()->id != "v1");
    CHECK(imported.audioConfig.front()->id != "a1");

    // the target's references must follow the remapped ids
    REQUIRE(imported.targets.front()->videoConfig.has_value());
    REQUIRE(imported.targets.front()->audioConfig.has_value());
    CHECK(*imported.targets.front()->videoConfig == imported.videoConfig.front()->id);
    CHECK(*imported.targets.front()->audioConfig == imported.audioConfig.front()->id);
}

TEST_CASE("RemapImportedIds also separates duplicate ids within the imported set itself") {
    MultiOutputConfig existing; // empty

    MultiOutputConfig imported;
    auto t1 = std::make_shared<OutputTargetConfig>(); t1->id = "same";
    auto t2 = std::make_shared<OutputTargetConfig>(); t2->id = "same";
    imported.targets.push_back(t1);
    imported.targets.push_back(t2);

    RemapImportedIds(imported, existing);

    auto it = imported.targets.begin();
    auto& first = *it; ++it;
    auto& second = *it;
    CHECK(first->id != second->id);
}

TEST_CASE("ConfigFromJsonString reports parse errors via errorOut") {
    std::string err;
    auto cfg = ConfigFromJsonString("not valid json", &err);
    CHECK(cfg.targets.empty());
    CHECK_FALSE(err.empty());
}
