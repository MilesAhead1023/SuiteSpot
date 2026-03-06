#pragma once
#include <string>
#include <vector>

struct DefaultPackData {
    std::string code;
    std::string name;
    int shotCount;
    std::string description;
};

namespace DefaultPacks {
    inline const std::vector<DefaultPackData> FLICKS_PICKS = {
        {
            "CE79-F64D-344F-5F1E",
            "Aerial Shots - Redirect",
            44,
            "This pack focuses on redirecting aerial shots, an essential offensive move to have in your arsenal. The 44 shots are designed to challenge even the most experienced players."
        },
        {
            "FA24-B2B7-2E8E-193B",
            "Ultimate Warm-Up",
            50,
            "Complete warm-up routine for all skill levels. Covers a wide range of mechanics to get you ready for ranked play."
        },
        {
            "D7F8-FD53-98D1-DAFE",
            "Backboard Therapy",
            40,
            "This training pack is designed to help players master their aerials off of the backboard. Perfect for players looking to improve their offensive capabilities."
        },
        {
            "6EB1-79B2-33B8-681C",
            "Ground Shots",
            50,
            "This training pack contains 50 shots that cover just about any possible ground shot you could see in a game. It includes a variety of angles, distances, and speeds."
        },
        {
            "5A65-4073-F310-5495",
            "Wall to Air Dribble",
            3,
            "This training pack is designed to help you improve your wall to air dribbling skills. The shots are set up to roll perfectly up the wall, allowing you to practice your setup and control."
        },
        {
            "A503-264C-A7EB-D282",
            "Musty's Speed Flip Kickoff",
            3,
            "This pack is designed to help you practice the speedflip kickoff, a technique that allows you to reach supersonic speed quickly off the kickoff."
        },
        {
            "CEB6-7AF0-9A2E-B92D",
            "Flip Reset",
            10,
            "This training pack includes a variety of shots to help you practice and master the flip reset mechanic. The shots are designed to be realistic and game-like."
        },
        {
            "6CF3-4C0B-32B4-1AC7",
            "Power Shots",
            20,
            "This pack is designed to help players improve their power and accuracy when shooting. It includes 20 shots that vary in distance and angle."
        },
        {
            "2E23-ABD5-20C6-DBD4",
            "Saves",
            50,
            "This training pack includes 50 different saves that cover a wide range of situations. From simple shots to difficult redirects, this pack will help you improve your reaction time."
        },
        {
            "CAFC-FB3E-3C0F-B8F1",
            "Double Shot Playground",
            50,
            "This pack is designed for advanced players who are looking to perfect their aerial skills and double tap shots. The shots in this pack involve using the backboard as a self-setup tool."
        }
    };
}