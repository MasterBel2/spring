/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#ifndef _SCENE_H
#define _SCENE_H

/**
 * @brief Handles drawing associated with a CGameController object.
 */
class Scene {
public:
    /** 
     * @brief Performs a draw operation.
     * 
     * @return boolean value indicating whether a new frame is needed.
     */
    virtual bool Draw() { return true; }
    /** 
     * @brief Allows the scene to do an optional cleanup before 
     * the associated controller resigns.
     */
    virtual void End() {};
};

#endif /* Scene.h */