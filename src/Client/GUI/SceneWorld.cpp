#include <sstream>
#include <math.h>
#include "common.h"
#include "PseuGUI.h"
#include "PseuWoW.h"
#include "Scene.h"
#include "MapTile.h"
#include "MapMgr.h"
#include "ShTlTerrainSceneNode.h"
#include "MCamera.h"
#include "MInput.h"
#include "WorldSession.h"
#include "World.h"

#define MOUSE_SENSIVITY 0.5f



SceneWorld::SceneWorld(PseuGUI *g) : Scene(g)
{
    DEBUG(logdebug("SceneWorld: Initializing..."));
    debugmode = false;

    // store some pointers right now to prevent repeated ptr dereferencing later (speeds up code)
    gui = g;
    wsession = gui->GetInstance()->GetWSession();
    mapmgr = wsession->GetWorld()->GetMapMgr();

    // TODO: hardcoded for now, make this adjustable later
    float fogdist = 150;

    ILightSceneNode* light = smgr->addLightSceneNode(0, core::vector3df(0,0,0), SColorf(255, 255, 255, 255), 1000.0f);
    SLight ldata = light->getLightData();
    ldata.AmbientColor = video::SColorf(0.2f,0.2f,0.2f);
    ldata.DiffuseColor = video::SColorf(1.0f,1.0f,1.0f);
    ldata.Type = video::ELT_DIRECTIONAL;
    ldata.Position = core::vector3df(-10,5,-5);
    light->setLightData(ldata);

    eventrecv = new MyEventReceiver();
    device->setEventReceiver(eventrecv);

    camera = new MCameraFPS(smgr);
    camera->setNearValue(0.1f);
    camera->setFarValue(12000); // TODO: adjust

    debugText = guienv->addStaticText(L"< debug text >",rect<s32>(0,0,driver->getScreenSize().Width,30),true,true,0,-1,true);

    smgr->addSkyDomeSceneNode(driver->getTexture("data/misc/sky.jpg"),64,64,1.0f,2.0f);    

    driver->setFog(video::SColor(0,100,101,190), true, fogdist, fogdist + 30, 0.02f);

    // setup cursor
    device->getCursorControl()->setVisible(false);

    InitTerrain();
    UpdateTerrain();

    DEBUG(logdebug("SceneWorld: Init done!"));
}

void SceneWorld::OnUpdate(s32 timediff)
{
    static position2d<s32> mouse_pos;

    UpdateTerrain();

    bool mouse_pressed_left = eventrecv->mouse.left_pressed();
    bool mouse_pressed_right = eventrecv->mouse.right_pressed();
    float timediff_f = timediff / 1000.0f;

    if(eventrecv->key.pressed(KEY_KEY_W) || (mouse_pressed_left && mouse_pressed_right))
        camera->moveForward(50 * timediff_f);
    if(eventrecv->key.pressed(KEY_KEY_S))
        camera->moveBack(50 * timediff_f);
    if(eventrecv->key.pressed(KEY_KEY_E))
        camera->moveRight(50 * timediff_f);
    if(eventrecv->key.pressed(KEY_KEY_Q))
        camera->moveLeft(50 * timediff_f);

    // if right mouse button pressed, move in axis, if not, turn camera
    if(eventrecv->key.pressed(KEY_KEY_D))
    {
        if(mouse_pressed_right)
            camera->moveRight(50 * timediff_f);
        else
            camera->turnRight(timediff_f * M_PI * 25);
    }
    if(eventrecv->key.pressed(KEY_KEY_A))
    {
        if(mouse_pressed_right)
            camera->moveLeft(50 * timediff_f);
        else
            camera->turnLeft(timediff_f * M_PI * 25);
    }
    
    if(eventrecv->key.pressed_once(KEY_BACK))
    {
        debugmode = !debugmode;
        if(debugmode)
        {
            terrain->setDebugDataVisible(EDS_FULL);
        }
        else
        {
            terrain->setDebugDataVisible(EDS_OFF);
        }
    }

    if(eventrecv->key.pressed_once(KEY_INSERT))
    {
        IImage *scrnshot = driver->createScreenShot();
        if(scrnshot)
        {
            CreateDir("screenshots");
            std::string date = getDateString();
            for(uint32 i = 0; i < date.length(); i++)
            {
                if(date[i] == ':')
                    date[i] = '_';
            }
            if(date[date.length()-1] == ' ')
            {
                date = date.substr(0,date.length()-2);
            }
            driver->writeImageToFile(scrnshot, ("screenshots/PseuWoW " + date + ".jpg").c_str(), device->getTimer()->getRealTime());
            scrnshot->drop();
        }
    }

    if(mouse_pressed_left || mouse_pressed_right)
    {
        //if(device->getCursorControl()->isVisible())
            //device->getCursorControl()->setVisible(false);
        if(mouse_pos != device->getCursorControl()->getPosition())
        {
            camera->turnRight(MOUSE_SENSIVITY * (device->getCursorControl()->getPosition().X - mouse_pos.X));
            // check if new camera pitch would cause camera to flip over; if thats the case keep current pitch
            f32 upval = MOUSE_SENSIVITY * (device->getCursorControl()->getPosition().Y - mouse_pos.Y);
            f32 newval = camera->getPitch() + upval;
            if( newval > 270.1f || newval < 89.9f)
            {
                camera->turnUp(upval);
            }
            device->getCursorControl()->setPosition(mouse_pos);
        }
    }
    else
    {
        device->getCursorControl()->setPosition(device->getCursorControl()->getPosition());
        //if(!device->getCursorControl()->isVisible())
            //device->getCursorControl()->setVisible(true);
        mouse_pos = device->getCursorControl()->getPosition();
    }
    
    // camera height control
    if (eventrecv->mouse.wheel < 10) eventrecv->mouse.wheel = 10;
    camera->setHeight(  eventrecv->mouse.wheel + terrain->getHeight(camera->getPosition())  );

    core::stringw str = L"Camera: pitch:";
    str += camera->getPitch();
    str += L"  dir:";
    str += camera->getDirection().X;
    str += L",";
    str += camera->getDirection().Y;
    str += L"  Pos: ";
    str = (((((str + camera->getPosition().X) + L" | ") + camera->getPosition().Y) + L" | ") + camera->getPosition().Z); 
    str += L"  -- Terrain: Sectors: ";
    str += (int)terrain->getSectorsRendered();
    str += L" / ";
    str += (int)terrain->getSectorCount();
    debugText->setText(str.c_str());

}

void SceneWorld::OnDraw(void)
{
    // draw all objects
    gui->domgr.Update(); // iterate over DrawObjects, draw them and clean up
}

void SceneWorld::OnDelete(void)
{
    DEBUG(logdebug("~SceneWorld()"));
}

void SceneWorld::InitTerrain(void)
{
    if(!mapmgr)
    {
        logerror("SceneWorld: MapMgr not present, cant create World GUI. Switching back GUI to idle.");
        gui->SetSceneState(SCENESTATE_GUISTART);
        return;
    }

    mapsize = 9 * 16 * 3; // 9 height floats in 16 chunks per tile per axis in 3 MapTiles
    tilesize = UNITSIZE;
    meshsize = CHUNKSIZE*3;
    vector3df terrainPos(0.0f, 0.0f, 0.0f); // TODO: use PseuWoW's world coords here?

    camera->setPosition(core::vector3df(mapsize*tilesize/2, 0, mapsize*tilesize/2) + terrainPos);

    terrain = new ShTlTerrainSceneNode(smgr,mapsize,mapsize,tilesize,meshsize);
    terrain->drop();
    terrain->follow(camera->getNode());
    terrain->setMaterialTexture(0, driver->getTexture("data/misc/dirt_test.jpg"));
    terrain->setMaterialFlag(video::EMF_LIGHTING, true);
    terrain->setMaterialFlag(video::EMF_FOG_ENABLE, true);
    terrain->setPosition(terrainPos);

}


void SceneWorld::UpdateTerrain(void)
{
    // check if we changed the maptile
    if(map_gridX == mapmgr->GetGridX() && map_gridY == mapmgr->GetGridY())
        return; // grid not changed, not necessary to update tile data        

    // ... if changed, do necessary stuff...
    map_gridX = mapmgr->GetGridX();
    map_gridY = mapmgr->GetGridY();

    // TODO: better to do this with some ZThread Condition or FastMutex, but dont know how to. help plz! [FG]
    if(!mapmgr->Loaded())
    {
        logdebug("SceneWorld: Waiting until maps are loaded...");
        while(!mapmgr->Loaded())
            device->sleep(1);
    }

    // TODO: as soon as WMO-only worlds are implemented, remove this!!
    if(!mapmgr->GetLoadedMapsCount())
    {
        logerror("SceneWorld: Error: No maps loaded, not able to draw any terrain. Switching back GUI to idle.");
        logerror("SceneWorld: Hint: Be sure you are not in an WMO-only world (e.g. human capital city or most instances)!");
        gui->SetSceneState(SCENESTATE_GUISTART);
        return;
    }

    // something is not good here. we have terrain, but the chunks are read incorrectly.
    // need to find out where which formula is wrong
    // the current terrain renderer code is just a test to see if ADT files are read correctly.
    // EDIT: it seems to display fine now, but i am still not sure if the way it is done is correct...
    mutex.acquire(); // prevent other threads from deleting the maptile
    logdebug("SceneWorld: Displaying MapTiles near grids x:%u y:%u",mapmgr->GetGridX(),mapmgr->GetGridY());
    for(s32 tiley = 0; tiley < 3; tiley++)
    {
        for(s32 tilex = 0; tilex < 3; tilex++)
        {
            MapTile *maptile = mapmgr->GetNearTile(tilex - 1, tiley - 1);
            if(maptile)
            {
                // apply map height data
                for(uint32 chy = 0; chy < 16; chy++)
                    for(uint32 chx = 0; chx < 16; chx++)
                    {
                        MapChunk *chunk = maptile->GetChunk(chx, chy);
                        for(uint32 hy = 0; hy < 9; hy++)
                        {
                            for(uint32 hx = 0; hx < 9; hx++)
                            {
                                f32 h = chunk->hmap_rough[hx * 9 + hy] + chunk->baseheight; // not sure if hx and hy are used correctly here
                                u32 terrainx = (144 * tiley) + (9 * chx) + hx;
                                u32 terrainy = (144 * tilex) + (9 * chy) + hy;
                                terrain->setHeight(terrainx, terrainy, h);
                            }
                        }
                    }
            }
            else
            {
                logerror("SceneWorld: MapTile not loaded, can't apply heightmap!");
            }
        }
    }
    mutex.release();

    // find out highest/lowest spot
    f32 highest = terrain->getHeight(0,0);
    f32 lowest = terrain->getHeight(0,0);
    f32 curheight;
    for(s32 j=0; j<terrain->getSize().Height+1; j++)
        for(s32 i=0; i<terrain->getSize().Width+1; i++)
        {
            curheight = terrain->getHeight(i,j);
            highest = max(highest,curheight);
            lowest = min(lowest,curheight);
        }
    f32 heightdiff = highest - lowest;

    // randomize terrain color dependng on height
    for(s32 j=0; j<terrain->getSize().Height+1; j++)
        for(s32 i=0; i<terrain->getSize().Width+1; i++)
        {
            curheight = terrain->getHeight(i,j);
            u32 g = (curheight / highest * 120) + 125;
            u32 r = (curheight / highest * 120) + 60;
            u32 b = (curheight / highest * 120) + 60;

            terrain->setColor(i,j, video::SColor(255,r,g,b));
        }

    logdebug("SceneWorld: Smoothing terrain normals...");
    terrain->smoothNormals();
}