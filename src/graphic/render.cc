/*
 * Copyright (C) 2002-2004, 2007-2010 by the Widelands Development Team
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

/*
Rendering functions of the 16-bit software renderer.
*/

#include "graphic.h"

#include "logic/building.h"
#include "economy/flag.h"
#include "economy/road.h"
#include "logic/editor_game_base.h"
#include "io/filesystem/filesystem.h"
#include "io/filesystem/layered_filesystem.h"
#include "log.h"
#include "logic/map.h"
#include "wui/minimap.h"
#include "logic/player.h"
#include "upcast.h"
#include "wexception.h"
#include "logic/world.h"

#include <SDL.h>

using Widelands::Flag;
using Widelands::PlayerImmovable;
using Widelands::Road;

/*
 * Create a Surface from a surface
 */
Surface::Surface(Surface const & other):

// HACK: assume this should be picture format; there is no SDL_CopySurface
m_surface     (SDL_DisplayFormatAlpha(other.m_surface)),

#ifdef HAS_OPENGL
m_surf_type  (SURF_SDL),
m_texture (0),
#endif 

m_offsx       (other.m_offsx),
m_offsy       (other.m_offsx),
m_w           (other.m_w),
m_h           (other.m_h)

{}


/*
 * Updating the whole Surface
 */
void Surface::update() {
	
#if HAS_OPENGL
	if (g_opengl && isGLsf()) {
		//log("Surface::update() SDL_GL_SwapBuffers\n");
		SDL_GL_SwapBuffers();
		//glClear(GL_COLOR_BUFFER_BIT);
		return;
	} 
#endif
	//  flip defaults to SDL_UpdateRect(m_surface, 0, 0, 0, 0);
	//in case of 2d graphics but also allows for double buffering
	SDL_Flip(m_surface);
}

/*
 * Save a bitmap
 */
void Surface::save_bmp(const char & fname) const {
	assert(m_surface);
#ifdef HAS_OPENGL
	if(g_opengl and isGLsf())
	{
		log("Warning: Surface::save_bmp() not implemented for opengl mode\n");
		return;
	}
#endif
	SDL_SaveBMP(m_surface, &fname);
}

/*
 * disable alpha for this surface. Needed
 * if you want to copy directly to the screen
 * by direct pixel access. For example for road
 * textures
 */
void Surface::force_disable_alpha() {
#ifdef HAS_OPENGL
	if(g_opengl and isGLsf())
	{
		log("Warning: Surface::force_disable_alpha() not implemented for opengl mode\n");
		return;
	}
#endif
	SDL_Surface * const newsur = SDL_DisplayFormat(m_surface);
	SDL_FreeSurface(m_surface);
	m_surface = newsur;
}

/*
===============
Draws the outline of a rectangle
===============
*/
void Surface::draw_rect(const Rect rc, const RGBColor clr) {
#ifdef HAS_OPENGL
	if(g_opengl and isGLsf())
	{
		//log("Surface::draw_rect() for opengl is experimental\n");
		glDisable(GL_BLEND);
		glBegin(GL_LINE_LOOP);
		    glColor3f
			((clr.r() / 256.0f),
			 (clr.g() / 256.0f),
			 (clr.b() / 256.0f));
		    glVertex2f(rc.x,        rc.y);
		    glVertex2f(rc.x + rc.w, rc.y);
		    glVertex2f(rc.x + rc.w, rc.y + rc.h);
		    glVertex2f(rc.x,        rc.y + rc.h);
		glEnd();
		return;
	}
#endif

	assert(m_surface);
	assert(rc.x >= 0);
	assert(rc.y >= 0);
	assert(rc.w >= 1);
	assert(rc.h >= 1);
	const uint32_t color = clr.map(format());

	const Point bl = rc.bottom_left() - Point(1, 1);

	for (int32_t x = rc.x + 1; x < bl.x; ++x) {
		set_pixel(x, rc.y, color);
		set_pixel(x, bl.y, color);
	}
	for (int32_t y = rc.y; y <= bl.y; ++y) {
		set_pixel(rc.x, y, color);
		set_pixel(bl.x, y, color);
	}
}


/*
===============
Draws a filled rectangle
===============
*/
void Surface::fill_rect(const Rect rc, const RGBAColor clr) {
	assert(m_surface);
	assert(rc.x >= 0);
	assert(rc.y >= 0);
	assert(rc.w >= 1);
	assert(rc.h >= 1);

#ifdef HAS_OPENGL
	if(g_opengl and isGLsf())
	{
		/*log("Surface::fill_rect((%d, %d, %d, %d),(%u, %u, %u)) for opengl\n",
		    rc.x, rc.y, rc.w, rc.h,
		    clr.r(), clr.g(), clr.b());*/
		glDisable(GL_BLEND);
		glBegin(GL_QUADS);
		    glColor4f
			((((GLfloat)clr.r) / 256.0f),
			 (((GLfloat)clr.g) / 256.0f),
			 (((GLfloat)clr.b) / 256.0f),
			 (((GLfloat)clr.a) / 256.0f));
		    glVertex2f(rc.x,        rc.y);
		    glVertex2f(rc.x + rc.w, rc.y);
		    glVertex2f(rc.x + rc.w, rc.y + rc.h);
		    glVertex2f(rc.x,        rc.y + rc.h);
		glEnd();
		return;
	}
#endif

	const uint32_t color = clr.map(format());

	SDL_Rect r = {rc.x, rc.y, rc.w, rc.h};
	SDL_FillRect(m_surface, &r, color);
}

/*
===============
Change the brightness of the given rectangle
This function is slow as hell.

* This function is a possible point to optimize on
  slow system. It takes a lot of cpu time atm and is
  not needed. It is used by the ui_basic stuff to
  highlight things.
===============
*/
void Surface::brighten_rect(const Rect rc, const int32_t factor) {
	if(!factor)
		return;
	assert(rc.x >= 0);
	assert(rc.y >= 0);
	assert(rc.w >= 1);
	assert(rc.h >= 1);

	const Point bl = rc.bottom_left();

	/*log
		("Surface::brighten_rect((%d, %d, %d, %d), %d)\n", 
		 bl.x, rc.x, bl.x, rc.x, factor);*/

#ifdef HAS_OPENGL
	if(g_opengl and isGLsf())
	{
		//log("Warning: Surface::brighten_rect() not implemented for opengl mode\n");
		/* glBlendFunc is a very nice feature of opengl. You can specify how the 
		 * color is calculated.
		 * 
		 * glBlendFunc(GL_ONE, GL_ONE) means the following:
		 * Rnew = Rdest + Rsrc 
		 * Gnew = Gdest + Gsrc
		 * Bnew = Bdest + Bsrc
		 * Anew = Adest + Asrc
		 * where Xnew is the new calculated color for destination, Xdest is the old 
		 * color of the destination and Xsrc is the color of the source.
		 */
		glEnable(GL_BLEND);
		glBlendFunc(GL_ONE, GL_ONE);
		
		// And now simply draw a rect with facor as the color (this is the source color)
		// over the region
		glBegin(GL_QUADS);
		    glColor3f
			((factor / 256.0f),
			 (factor / 256.0f),
			 (factor / 256.0f));
		    glVertex2f(rc.x,        rc.y);
		    glVertex2f(rc.x + rc.w, rc.y);
		    glVertex2f(rc.x + rc.w, rc.y + rc.h);
		    glVertex2f(rc.x,        rc.y + rc.h);
		glEnd();
		return;
	}
#endif

	lock();

	if(m_surface->format->BytesPerPixel == 4)
	{
		for (int32_t y = rc.y; y < bl.y; ++y) for (int32_t x = rc.x; x < bl.x; ++x) {
		  
			Uint8 * const pix =
				static_cast<Uint8 *>(m_surface->pixels) +
				(y + m_offsy)* m_surface->pitch + (x + m_offsx) * 4;
		  
			uint32_t const clr = *reinterpret_cast<const Uint32 *>(pix);
			uint8_t gr, gg, gb;
			SDL_GetRGB(clr, m_surface->format, &gr, &gg, &gb);
			int16_t r = gr + factor;
			int16_t g = gg + factor;
			int16_t b = gb + factor;
		
			if (b & 0xFF00)
				b = ~b >> 24;
			if (g & 0xFF00)
				g = ~g >> 24;
			if (r & 0xFF00)
				r = ~r >> 24;
		
			*reinterpret_cast<Uint32 *>(pix) = SDL_MapRGB(m_surface->format, r, g, b);
		}
	} else if(m_surface->format->BytesPerPixel == 2) {
		for (int32_t y = rc.y; y < bl.y; ++y) for (int32_t x = rc.x; x < bl.x; ++x) {
		  
			Uint8 * const pix =
				static_cast<Uint8 *>(m_surface->pixels) +
				(y + m_offsy)* m_surface->pitch + (x + m_offsx) * 2;
		  
			uint32_t const clr = *reinterpret_cast<const Uint16 *>(pix);
			uint8_t gr, gg, gb;
			SDL_GetRGB(clr, m_surface->format, &gr, &gg, &gb);
			int16_t r = gr + factor;
			int16_t g = gg + factor;
			int16_t b = gb + factor;
		
			if (b & 0xFF00)
				b = ~b >> 24;
			if (g & 0xFF00)
				g = ~g >> 24;
			if (r & 0xFF00)
				r = ~r >> 24;
		
			*reinterpret_cast<Uint16 *>(pix) = SDL_MapRGB(m_surface->format, r, g, b);
		}
	  
	}

	unlock();
}


/*
===============
Clear the entire bitmap to black
===============
*/
void Surface::clear() {
#ifdef HAS_OPENGL
	if(g_opengl and isGLsf())
	{
		//log("Surface::clear() for opengl mode\n");
 		glClearColor( 0.0f, 0.0f, 0.0f, 1.0f );
		glClear(GL_COLOR_BUFFER_BIT);
		return;
	}
#endif

	SDL_FillRect(m_surface, 0, 0);
}

/*
===============

===============
*/
#ifdef HAS_OPENGL
GLuint Surface::getTexture()
{
	if(!m_glTexUpdate and m_texture)
		return m_texture->id();
	
	//log("Warning: Surface::getTexture() creates Texture from Surface\n");

	GLuint texture;
	SDL_Surface *surface;
	GLenum pixels_format, pixels_type;
	GLint  Bpp;

	surface=m_surface;

	SDL_Surface * tsurface = NULL;
	SDL_PixelFormat & fmt = *surface->format;
	
	if(fmt.palette)
	{
		log("Warning: trying to use a paletted picture for opengl texture\n");
		SDL_Surface * tsurface = SDL_DisplayFormatAlpha(surface);
		SDL_SetAlpha(tsurface, 0, 0);
		SDL_SetAlpha(tsurface, 0, 0);
		SDL_BlitSurface(surface, 0, tsurface, 0);
		surface = tsurface;
		fmt = *surface->format;
	}

	if(fmt.colorkey!=0)
	{
		log("Colorkey: %X Alpha: %u, Amask: %X\n",
		fmt.colorkey,
		fmt.alpha,
		fmt.Amask);

		SDL_Surface * tsurface = SDL_CreateRGBSurface(SDL_SWSURFACE, surface->w, surface->h, 32, 
                                  0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000);
		//DisplayFormatAlpha(surface);
		//SDL_SetAlpha(tsurface, 0, 255);
		//SDL_SetAlpha(tsurface, 0, 0);
		SDL_BlitSurface(surface, 0, tsurface, 0);
		surface = tsurface;
		fmt = *surface->format;
	}

	Bpp = fmt.BytesPerPixel;

	log
		("Surface::getTexture() Size: (%d, %d) %db(%dB) ", get_w(), get_h(),
		 fmt.BitsPerPixel, Bpp);

	log("R:%X, G:%X, B:%X, A:%X", fmt.Rmask, fmt.Gmask, fmt.Bmask, fmt.Amask);
		 
	if(Bpp==4)
	{
		if(fmt.Rmask==0x000000ff and fmt.Gmask==0x0000ff00 and fmt.Bmask==0x00ff0000)
		{
			if(fmt.Amask==0xff000000)
			{ pixels_format=GL_RGBA; log(" RGBA 8888 "); }
			else
			{ pixels_format=GL_RGB; log(" RGB 8880 "); }
		}
		else if(fmt.Bmask==0x000000ff and fmt.Gmask==0x0000ff00 and fmt.Rmask==0x00ff0000)
		{
			if(fmt.Amask==0xff000000)
			{ pixels_format=GL_BGRA; log(" RGBA 8888 "); }
			else
			{ pixels_format=GL_BGR; log(" RGBA 8888 "); }
		}

		pixels_type=GL_UNSIGNED_INT_8_8_8_8_REV;
	}
	else if (Bpp==2)
	{
		if((fmt.Rmask==0xF800) and (fmt.Gmask==0x7E0) and (fmt.Bmask==0x1F))
		{
			pixels_format=GL_RGB; log(" RGB 565"); 
			 // A:0
		}
		else if ((fmt.Bmask==0xF800) and (fmt.Gmask==0x7E0) and (fmt.Rmask==0x1F))
		{
			pixels_format=GL_BGR; log(" BGR 565"); 
		} else
			assert(false);
		pixels_type = GL_UNSIGNED_SHORT_5_6_5;
	}
	else
		assert(false);
	log("\n");

	// Let OpenGL create a texture object
	glGenTextures( 1, &texture );

	// seclet the texture object
	glBindTexture( GL_TEXTURE_2D, texture );

	// set texture filter to siply take the nearest pixel.
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );

	SDL_LockSurface(surface);

	glTexImage2D( GL_TEXTURE_2D, 0, Bpp, surface->w, surface->h, 0,
	pixels_format, pixels_type, surface->pixels );
	SDL_UnlockSurface(surface);
	
	if(tsurface)
		SDL_FreeSurface(tsurface);

	//SDL_FreeSurface(m_surface);
	if(m_texture)
		delete m_texture;
	m_texture = new oglTexture(texture);
	m_glTexUpdate = false;
	return texture;
}
#endif

void Surface::blit(Point const dst, Surface * const src, Rect const srcrc, bool enable_alpha)
{
#ifdef HAS_OPENGL
	if(g_opengl and isGLsf())
	{
		/* Set a texture scaling factor. Normaly texture coordiantes 
		 * (see glBegin()...glEnd() Block below) are given in the range 0-1
		 * to avoid the calculation (and let opengl do it) the texture 
		 * space is modified. glMatrixMode select which matrix to manipulate
		 * (the texture transformation matrix in this case). glLoadIdentity()
		 * resets the (selected) matrix to the identity matrix. And finally 
		 * glScalef() calculates the texture matrix.
		 */
		glMatrixMode(GL_TEXTURE);
		glLoadIdentity();
		glScalef(1.0f/(GLfloat)src->get_w(), 1.0f/(GLfloat)src->get_h(), 1);

		// Enable Alpha blending 
		if(enable_alpha) {
			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		} else
			glDisable(GL_BLEND);

		/* select the texture to paint on the screen
		 * openGL does not know anything about SDL_Surfaces
		 * opengl uses textures to handle images
		 * getTexture() returns the texture id of the Surface. It creates
		 * the texture from the SDL_Surface if it doesn't exist
		 */
		glBindTexture( GL_TEXTURE_2D, src->getTexture());
		
		/* This block between blBegin() and glEnd() does the blit.
		 * It draws a textured rectangle. glTexCoord2i() set the Texture
		 * Texture cooardinates. This is the source rectangle.
		 * glVertex2f() sets the screen coordiantes which belong to the 
		 * previous texture coordinate. This is the destination rectangle 
		 */
		glBegin(GL_QUADS);
		    //set color white, otherwise textures get mixed with color
		    glColor3f(1.0,1.0,1.0);
		    //top-left 
		    glTexCoord2i( srcrc.x,         srcrc.y );
		    glVertex2i(   dst.x,           dst.y );
		    //top-right
		    glTexCoord2i( srcrc.x+srcrc.w, srcrc.y );
		    glVertex2f(   dst.x+srcrc.w,   dst.y );
		    //botton-right
		    glTexCoord2i( srcrc.x+srcrc.w, srcrc.y+srcrc.h );
		    glVertex2f(   dst.x+srcrc.w,   dst.y+srcrc.h );
		    //bottom-left
		    glTexCoord2i( srcrc.x,         srcrc.y + srcrc.h);
		    glVertex2f(   dst.x,           dst.y+srcrc.h );
		glEnd();

		glMatrixMode(GL_TEXTURE);
		glLoadIdentity();

		return;
	}
#endif

	SDL_Rect srcrect = {srcrc.x, srcrc.y, srcrc.w, srcrc.h};
	SDL_Rect dstrect = {dst.x, dst.y, 0, 0};

	//SDL_SetAlpha(src->m_surface, enable_alpha && SDL_SRCALPHA, 0);
	//SDL_SetAlpha(m_surface, enable_alpha && SDL_SRCALPHA, 0);

	SDL_BlitSurface(src->m_surface, &srcrect, m_surface, &dstrect);
}

/*
 * Fast blit, simply copy the source to the destination
 */
void Surface::fast_blit(Surface * const src) {
#ifdef HAS_OPENGL
	if(g_opengl and isGLsf())
	{
		log("Warning: Surface::fast_blit() not implemented for opengl mode\n");
		return;
	}
#endif
	SDL_BlitSurface(src->m_surface, 0, m_surface, 0);
}

/*
 * Blend to colors; only needed for calc_minimap_color below
 */
inline static uint32_t blend_color
	(SDL_PixelFormat const &       format,
	 uint32_t                const clr1,
	 Uint8 const r2, Uint8 const g2, Uint8 const b2)
{
	Uint8 r1, g1, b1;
	SDL_GetRGB(clr1, &const_cast<SDL_PixelFormat &>(format), &r1, &g1, &b1);
	return
		SDL_MapRGB
			(&const_cast<SDL_PixelFormat &>(format),
			 (r1 + r2) / 2, (g1 + g2) / 2, (b1 + b2) / 2);
}

/*
===============
calc_minimap_color

Return the color to be used in the minimap for the given field.
===============
*/
inline static uint32_t calc_minimap_color
	(SDL_PixelFormat             const &       format,
	 Widelands::Editor_Game_Base const &       egbase,
	 Widelands::FCoords                  const f,
	 uint32_t                            const flags,
	 Widelands::Player_Number            const owner,
	 bool                                const see_details)
{
	uint32_t pixelcolor = 0;

	if (flags & MiniMap::Terrn) {
		pixelcolor =
			g_gr->
			get_maptexture_data
				(egbase.map().world()
				 .terrain_descr(f.field->terrain_d()).get_texture())
			->get_minimap_color(f.field->get_brightness());
	}

	if (flags & MiniMap::Owner) {
		if (0 < owner) { //  If owned, get the player's color...
			const RGBColor * const playercolors =
				egbase.player(owner).get_playercolor();

			//  ...and add the player's color to the old color.
			pixelcolor = blend_color
				(format,
				 pixelcolor,
				 playercolors[3].r(),  playercolors[3].g(), playercolors[3].b());
		}
	}

	if (see_details)
		if (upcast(PlayerImmovable const, immovable, f.field->get_immovable())) {
			if (flags & MiniMap::Roads and dynamic_cast<Road const *>(immovable))
				pixelcolor = blend_color(format, pixelcolor, 255, 255, 255);
			if
				((flags & MiniMap::Flags and dynamic_cast<Flag const *>(immovable))
				 or
				 (flags & MiniMap::Bldns
				  and
				  dynamic_cast<Widelands::Building const *>(immovable)))
				pixelcolor =
					SDL_MapRGB
						(&const_cast<SDL_PixelFormat &>(format), 255, 255, 255);
		}

	return pixelcolor;
}

template<typename T>
static void draw_minimap_int
	(Uint8                             * const pixels,
	 uint16_t                            const pitch,
	 SDL_PixelFormat             const &       format,
	 uint32_t                            const mapwidth,
	 Widelands::Editor_Game_Base const &       egbase,
	 Widelands::Player           const * const player,
	 Rect                                const rc,
	 Point                               const viewpoint,
	 uint32_t                            const flags)
{
	Widelands::Map const & map = egbase.map();
	if (not player or player->see_all()) for (uint32_t y = 0; y < rc.h; ++y) {
		Uint8 * pix = pixels + (rc.y + y) * pitch + rc.x * sizeof(T);
		Widelands::FCoords f
			(Widelands::Coords
			 	(viewpoint.x, viewpoint.y + (flags & MiniMap::Zoom2 ? y / 2 : y)));
		map.normalize_coords(f);
		f.field = &map[f];
		Widelands::Map_Index i = Widelands::Map::get_index(f, mapwidth);
		for (uint32_t x = 0; x < rc.w; ++x, pix += sizeof(T)) {
			if (x % 2 || !(flags & MiniMap::Zoom2))
				move_r(mapwidth, f, i);
			*reinterpret_cast<T *>(pix) = static_cast<T>
				(calc_minimap_color
				 	(format, egbase, f, flags, f.field->get_owned_by(), true));
		}
	} else {
		Widelands::Player::Field const * const player_fields = player->fields();
		for (uint32_t y = 0; y < rc.h; ++y) {
			Uint8 * pix = pixels + (rc.y + y) * pitch + rc.x * sizeof(T);
			Widelands::FCoords f;
			if (flags & MiniMap::Zoom2)
				f = Widelands::FCoords
					(Widelands::Coords(viewpoint.x, viewpoint.y + y / 2), 0);
			else
				f = Widelands::FCoords
					(Widelands::Coords(viewpoint.x, viewpoint.y + y), 0);
			map.normalize_coords(f);
			f.field = &map[f];
			Widelands::Map_Index i = Widelands::Map::get_index(f, mapwidth);
			for (uint32_t x = 0; x < rc.w; ++x, pix += sizeof(T)) {
				if (x % 2 || !(flags & MiniMap::Zoom2))
					move_r(mapwidth, f, i);
				Widelands::Player::Field const & player_field = player_fields[i];
				Widelands::Vision const vision = player_field.vision;

				*reinterpret_cast<T *>(pix) =
					static_cast<T>
					(vision ?
					 calc_minimap_color
					 	(format, egbase, f, flags, player_field.owner, 1 < vision)
					 :
					 0);
			}
		}
	}
}

/*
===============
Draw a minimap into the given rectangle of the bitmap.
viewpt is the field at the top left of the rectangle.
===============
*/
void Surface::draw_minimap
	(Widelands::Editor_Game_Base const &       egbase,
	 Widelands::Player           const * const player,
	 Rect                                const rc,
	 Point                               const viewpt,
	 uint32_t                            const flags)
{
#ifdef HAS_OPENGL
	SDL_Surface * surface = SDL_CreateRGBSurface
				(SDL_SWSURFACE,
				 rc.w,
				 rc.h,
				 m_surface->format->BitsPerPixel,
				 m_surface->format->Rmask,
				 m_surface->format->Gmask,
				 m_surface->format->Bmask,
				 m_surface->format->Amask);

	/*log("Surface::draw_minimap(Rect(%d, %d, d%, %d), Point(%d, %d))\n",
	    rc.x, rc.y, rc.w, rc.h, viewpt.x, viewpt.y);*/

	Rect rc2;
	rc2.x=rc2.y=0;
	rc2.w=rc.w;
	rc2.h=rc.h;
	SDL_FillRect(surface, 0, SDL_MapRGBA(surface->format, 0, 0, 0, 255));
	SDL_LockSurface(surface);
	
	Uint8 * const pixels = static_cast<uint8_t *>(surface->pixels);
	Widelands::X_Coordinate const w = egbase.map().get_width();
	switch (surface->format->BytesPerPixel) {
	case sizeof(Uint16):
		draw_minimap_int<Uint16>
			(pixels, surface->pitch, *surface->format, w, egbase, player, rc2, viewpt, flags);
		break;
	case sizeof(Uint32):
		draw_minimap_int<Uint32>
			(pixels, surface->pitch, *surface->format, w, egbase, player, rc2, viewpt, flags);
		break;
	default:
		assert (false);
	}

	SDL_UnlockSurface(surface);

	Surface surf;
	surf.set_sdl_surface(*surface);

	blit(Point(rc.x,rc.y), &surf, rc2, false);
#else
	//TODO: this const_cast is evil and should be exorcised.
	Uint8 * const pixels = const_cast<Uint8 *>
		(static_cast<const Uint8 *>(get_pixels()));

	const uint16_t pitch = get_pitch();
	Widelands::X_Coordinate const w = egbase.map().get_width();
	switch (format().BytesPerPixel) {
	case sizeof(Uint16):
		draw_minimap_int<Uint16>
			(pixels, pitch, format(), w, egbase, player, rc, viewpt, flags);
		break;
	case sizeof(Uint32):
		draw_minimap_int<Uint32>
			(pixels, pitch, format(), w, egbase, player, rc, viewpt, flags);
		break;
	default:
		assert (false);
	}
#endif
}

/*
==============================================================================

AnimationGfx -- contains graphics data for an animtion

==============================================================================
*/

/*
===============
AnimationGfx::AnimationGfx

Load the animation
===============
*/
static const uint32_t nextensions = 2;
static const char extensions[nextensions][5] = {".png", ".jpg"};
AnimationGfx::AnimationGfx(AnimationData const * const data) :
	m_hotspot(data->hotspot)
{
	m_encodedata.hasplrclrs = data->encdata.hasplrclrs;
	m_encodedata.plrclr[0]  = data->encdata.plrclr[0];
	m_encodedata.plrclr[1]  = data->encdata.plrclr[1];
	m_encodedata.plrclr[2]  = data->encdata.plrclr[2];
	m_encodedata.plrclr[3]  = data->encdata.plrclr[3];

	//  In the filename template, the last sequence of '?' characters (if any)
	//  is replaced with a number, for example the template "idle_??" is
	//  replaced with "idle_00". Then the code looks if there is a file with
	//  that name + any extension in the list above. Assuming that it finds a
	//  file, it increments the number so that the filename is "idle_01" and
	//  looks for a file with that name + extension, and so on until it can not
	//  find any file. Then it is assumed that there are no more frames in the
	//  animation.

	//  Allocate a buffer for the filename. It must be large enough to hold the
	//  picture name template, the "_pc" (3 characters) part for playercolor
	//  masks, the extension (4 characters including the dot) and the null
	//  terminator. Copy the picture name template into the buffer.
	char filename[256];
	std::string::size_type const picnametempl_size = data->picnametempl.size();
	if (sizeof(filename) < picnametempl_size + 3 + 4 + 1)
		throw wexception
			("buffer too small (%u) for picture name temlplate of size %u\n",
			 sizeof(filename), picnametempl_size);
	strcpy(filename, data->picnametempl.c_str());

	//  Find out where in the picture name template the number is. Search
	//  backwards from the end.
	char * const after_basename = filename + picnametempl_size;
	char * last_digit = after_basename;
	while (filename <= last_digit and *last_digit != '?')
		--last_digit;
	char * before_first_digit = last_digit;
	while (filename <= before_first_digit and *before_first_digit == '?') {
		*before_first_digit = '0';
		--before_first_digit;
	}
	int width = 0, height;
#ifndef NDEBUG
	//#define VALIDATE_ANIMATION_CROPPING
#endif
#ifdef VALIDATE_ANIMATION_CROPPING
	bool data_in_x_min = false, data_in_x_max = false;
	bool data_in_y_min = false, data_in_y_max = false;
#endif

	for (;;) {
		// Load the base image
		for (size_t extnr = 0;;) {
			strcpy(after_basename, extensions[extnr]);
			if (g_fs->FileExists(filename)) { //  Is the frame actually there?
				try {
					SDL_Surface & surface = *LoadImage(filename);
					if (width == 0) { //  This is the first frame.
						width  = surface.w;
						height = surface.h;
					} else if (width != surface.w or height != surface.h)
						throw wexception
							("wrong size: (%u, %u), should be (%u, %u) like the "
							 "first frame",
							 surface.w, surface.h, width, height);
					//  Get a new AnimFrame.
					Surface & frame = *new Surface();
					m_plrframes[0].push_back(&frame);
					frame.set_sdl_surface(surface);
#ifdef VALIDATE_ANIMATION_CROPPING
					if (not data_in_x_min)
						for (int y = 0; y < height; ++y) {
							uint8_t r, g, b, a;
							SDL_GetRGBA
								(frame.get_pixel(0,         y),
								 surface.format,
								 &r, &g, &b, &a);
							if (a) {
								data_in_x_min = true;
								break;
							}
						}
					if (not data_in_x_max)
						for (int y = 0; y < height; ++y) {
							uint8_t r, g, b, a;
							SDL_GetRGBA
								(frame.get_pixel(width - 1, y),
								 surface.format,
								 &r, &g, &b, &a);
							if (a) {
								data_in_x_max = true;
								break;
							}
						}
					if (not data_in_y_min)
						for (int x = 0; x < width; ++x) {
							uint8_t r, g, b, a;
							SDL_GetRGBA
								(frame.get_pixel(x,         0),
								 surface.format,
								 &r, &g, &b, &a);
							if (a) {
								data_in_y_min = true;
								break;
							}
						}
					if (not data_in_y_max)
						for (int x = 0; x < width; ++x) {
							uint8_t r, g, b, a;
							SDL_GetRGBA
								(frame.get_pixel(x,         height - 1),
								 surface.format,
								 &r, &g, &b, &a);
							if (a) {
								data_in_y_max = true;
								break;
							}
						}
#endif
				} catch (std::exception const & e) {
					throw wexception
						("could not load animation frame %s: %s\n",
						 filename, e.what());
				}
				//  Successfully loaded the frame.
				break;  //  No need to look for files with other extensions.
			} else if (++extnr == nextensions) //  Tried all extensions.
				goto end;  //  This frame does not exist. No more frames in anim.
		}

		switch (m_encodedata.hasplrclrs) {
		case EncodeData::Mask:
			strcpy(after_basename, "_pc");
			for (size_t extnr = 0;;) {
				strcpy(after_basename + 3, extensions[extnr]);
				if (g_fs->FileExists(filename)) {
					try {
						SDL_Surface & surface = *LoadImage(filename);
						if (width != surface.w or height != surface.h)
							throw wexception
								("playercolor mask has wrong size: (%u, %u), should "
								 "be (%u, %u) like the animation frame",
								 surface.w, surface.h, width, height);
						Surface & frame = *new Surface();
						frame.set_sdl_surface(surface);
						m_pcmasks.push_back(&frame);
						break;
					} catch (std::exception const & e) {
						throw wexception
							("error while reading \"%s\": %s", filename, e.what());
					}
				} else if (++extnr == nextensions) {
					after_basename[3] = '\0'; //  cut off the extension
					throw wexception("\"%s\" is missing", filename);
				}
			}
			break;
		case EncodeData::Old: {
			Surface & origsurface = *m_plrframes[0].back();
			Surface & newsurface = *new Surface();
			newsurface.set_sdl_surface(*SDL_DisplayFormat(origsurface.m_surface));

			static const RGBColor whitecolors[4] = {
				RGBColor(119, 119, 119),
				RGBColor(166, 166, 166),
				RGBColor(210, 210, 210),
				RGBColor(255, 255, 255)
			};
			uint32_t white = RGBColor(255, 255, 255).map(newsurface.format());
			uint32_t black = RGBColor(0, 0, 0).map(newsurface.format());
			uint32_t plrclr[4];
			uint32_t new_plrclr[4];

			for (uint32_t i = 0; i < 4; ++i) {
				plrclr[i] = m_encodedata.plrclr[i].map(origsurface.format());
				new_plrclr[i] = whitecolors[i].map(origsurface.format());
			}

			//  Walk the surface, replace all playercolors.
			for (uint32_t y = 0; y < newsurface.get_h(); ++y) {
				for (uint32_t x = 0; x < newsurface.get_w(); ++x) {
					const uint32_t clr = origsurface.get_pixel(x, y);
					newsurface.set_pixel(x, y, black);
					for (uint32_t i = 0; i < 4; ++i) {
						if (clr == plrclr[i]) {
							origsurface.set_pixel(x, y, new_plrclr[i]);
							newsurface.set_pixel(x, y, white);
							break;
						}
					}
				}
			}

			m_pcmasks.push_back(&newsurface);
			break;
		}
		case EncodeData::No:;
		}

		//  Increment the number in the filename.
		for (char * digit_to_increment = last_digit;;) {
			if (digit_to_increment == before_first_digit)
				goto end; //  The number wrapped around to all zeros.
			assert('0' <= *digit_to_increment);
			assert        (*digit_to_increment <= '9');
			if (*digit_to_increment == '9') {
				*digit_to_increment = '0';
				--digit_to_increment;
			} else {
				++*digit_to_increment;
				break;
			}
		}
	}
end:
	if (m_plrframes[0].empty())
		throw wexception
			("animation %s has no frames", data->picnametempl.c_str());

	if (m_pcmasks.size() and m_pcmasks.size() < m_plrframes[0].size())
		throw wexception
			("animation has %zu frames but playercolor mask has only %zu frames",
			 m_plrframes[0].size(), m_pcmasks.size());
#ifdef VALIDATE_ANIMATION_CROPPING
	if
		(char const * const where =
		 	not data_in_x_min ? "left column"  :
		 	not data_in_x_max ? "right column" :
		 	not data_in_y_min ? "top row"      :
		 	not data_in_y_max ? "bottom row"   :
		 	0)
		log
			("The animation %s is not properly cropped (the %s has only fully "
			 "transparent pixels in each frame. Therefore the %s should be "
			 "removed (and the hotspot adjusted accordingly). Otherwise "
			 "rendering will be slowed down by needless painting of fully "
			 "transparent pixels.\n",
			 data->picnametempl.c_str(), where, where);
#endif
}


AnimationGfx::~AnimationGfx()
{
	for (Widelands::Player_Number i = 0; i <= MAX_PLAYERS; ++i) {
		std::vector<Surface *> & frames = m_plrframes[i];
		for (uint32_t j = 0; j < frames.size(); ++j)
			delete frames[j];
	}

	for (uint32_t j = 0; j < m_pcmasks.size(); ++j)
		delete m_pcmasks[j];
}


/*
===============
Encodes the given surface into a frame
===============
*/
void AnimationGfx::encode(uint8_t const plr, RGBColor const * const plrclrs)
{
	assert(m_plrframes[0].size() == m_pcmasks.size());
	std::vector<Surface *> & frames = m_plrframes[plr];

	for (uint32_t i = 0; i < m_plrframes[0].size(); ++i) {
		//  Copy the old surface.
		Surface & origsurface = *m_plrframes[0][i];
		Surface & newsurface = *new Surface();
		newsurface.set_sdl_surface
			(*SDL_DisplayFormatAlpha(origsurface.get_sdl_surface()));

		Surface & pcmask = *m_pcmasks[i];

		// This could be done significantly faster, but since we
		// cache the result, let's keep it simple for now.
		for (uint32_t y = 0; y < newsurface.get_h(); ++y) {
			for (uint32_t x = 0; x < newsurface.get_w(); ++x) {
				RGBAColor source;
				RGBAColor mask;

				source.set(newsurface.format(), newsurface.get_pixel(x, y));
				mask.set(pcmask.format(), pcmask.get_pixel(x, y));

				if
					(uint32_t const influence =
					 	static_cast<uint32_t>(mask.r) * mask.a)
				{
					uint32_t const intensity =
						(luminance_table_r[source.r] +
						 luminance_table_g[source.g] +
						 luminance_table_b[source.b] +
						 8388608U) //  compensate for truncation:  .5 * 2^24
						>> 24;
					RGBAColor plrclr;

					plrclr.r = (plrclrs[3].r() * intensity) >> 8;
					plrclr.g = (plrclrs[3].g() * intensity) >> 8;
					plrclr.b = (plrclrs[3].b() * intensity) >> 8;

					RGBAColor dest(source);
					dest.r =
						(plrclr.r * influence + dest.r * (65536 - influence)) >> 16;
					dest.g =
						(plrclr.g * influence + dest.g * (65536 - influence)) >> 16;
					dest.b =
						(plrclr.b * influence + dest.b * (65536 - influence)) >> 16;

					newsurface.set_pixel(x, y, dest.map(newsurface.format()));
				}
			}
		}

		frames.push_back(&newsurface);
	}
}
