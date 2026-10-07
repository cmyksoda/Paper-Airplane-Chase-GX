# Paper Airplane Chase GX

A Wii homebrew port of **Paper Airplane Chase**, with Endless, all eight Time Attack courses and local two-player Race, original gameplay code, music and sound effects, and a new title screen and pause menu.

|Title Screen|Pause Menu|Graphics Options|
|---|---|---|
|![Title Screen](assets/screenshots/title.png)|![Pause Menu](assets/screenshots/pause.png)|![Graphics Options](assets/screenshots/graphics.png)|

## Install

You’ll need the Homebrew Channel, an SD card, and your own **(USA)(EnFrEs)** DSiWare ROM.

1. Extract the release ZIP to the root of your SD card.
2. Put your `.nds` ROM in `sd:/apps/paperplane/`, beside `boot.dol`. Any filename is fine. Unzip it first if it’s in a ZIP.
3. Launch **Paper Airplane Chase GX** from the Homebrew Channel.

The app checks the ROM and prepares its data automatically on first launch. Later launches use the saved cache, so you're free to remove the ROM after first successful boot. Other regions and revisions aren’t supported.

## Controls

| Controller | Move | Select | Pause | Back / exit |
|---|---|---|---|---|
| Wii Remote, horizontal | D-pad | 1 / 2 | Plus | Home |
| Wii Remote, vertical | D-pad | A | Plus | Home |
| Wii Remote + Nunchuk | Stick | C / Z | Plus | Home |
| Classic Controller | D-pad / left stick | A | Plus | Home |
| GameCube Controller | D-pad / stick | A | Start | Z |

In menus, select with A / 2 and go back with B. Left/right chooses a game; up/down chooses a Time Attack course. Pressing A on a bare Wii Remote selects vertical controls; pressing 1 or 2 selects horizontal controls. B keeps the current grip.

In Endless and Time Attack, press a button or move the stick on another controller to switch all controls to it, including during play.

In Race, whoever starts the game is Player 1. Press a button or move the stick/D-pad on another controller to join as Player 2 and start the countdown. Idle controllers cannot claim a slot. Release any input held before Race started and press it again to join. Assignments stay fixed through restarts until returning to the title. Either player can pause; Player 1 operates the menus. Disconnecting an assigned controller pauses the race.

## Display and saves

Open **Pause → Graphics Options** to toggle **240p at 60 Hz** and choose between **Fixed and Fill** scaling.

Fixed keeps the original proportions. Unlike Bird & Beans GX, gameplay shows two original screens together: a continuous 256×384 tower in single-player, or two 256×192 views side by side in Race. In normal 4:3 output, Fixed gameplay uses the original pixels at **1×**, while the menus use **2×**. Doubling the entire gameplay view would not fit on screen. At 240p, gameplay is reduced vertically; it is not a pixel-perfect 2× mode.

240p keeps the full gameplay view and blends source rows when reducing it, so thin scrolling ledges are not discarded on alternate rows. Menus keep complete font rows at the Fixed size.

Fill enlarges the gameplay composition to use more of the screen and follows the Wii’s 4:3 or 16:9 setting. Scaling applies to gameplay; menus always stay at the Fixed size, and the title and setup screens always use the full screen.

Widescreen systems also show **Aspect Ratio** for the option to use a centered 4:3 view while in Fill mode. Widescreen aspect correction resamples horizontally, so the exact 1× gameplay / 2× menu pixel grids apply to normal 4:3 output.

All graphics options are saved when you leave the menu and restored on the next launch.

Graphics options, the Endless high score and best completed time for each of the eight Time Attack courses are saved in `apps/paperplane/scores.dat`. Race does not save win totals. Records are saved on game over, restart, return to title or a clean exit. Keep that file when updating. Powering off during a run can lose an unsaved record.

## License

The port code and documentation are **GPLv3**. Nintendo game data and the original Nintendo logo are excluded; third-party components retain their own licenses. See [license notices](licenses/README.md).

For the project logo, I only claim ownership of my modification: the **GX** addition. The original **Paper Airplane Chase** logo is Nintendo’s property. See [artwork attribution](licenses/Artwork.txt).

*This project was made with AI assistance. For more information, see [my AI usage statement](https://github.com/cmyksoda/cmyksoda/blob/main/AI_USAGE.md).*
