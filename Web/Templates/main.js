// Starts the Web player: the data folder next to the page (?data=<URL> replaces it) fetched into /Data by the loader of
// Helix.Platform before main, the .NET runtime with the player linked into it, ?args= on the command line. The player's
// frames run on the browser's loop after main returns.
import { dotnet } from './_framework/dotnet.js';
import { helixArguments, helixPreload } from './HelixPreload.mjs';

const status = document.getElementById('status');
const canvas = document.getElementById('canvas');
try {
  const runtime = await dotnet
    .withModuleConfig({
      canvas,
      preRun: [helixPreload({ url: 'Data', mount: '/Data', files: ['settings.asset'], lists: ['assets.index', 'Managed/assemblies.index'], companions: { '.dll': '.pdb' } })],
      printErr: (text) => { console.error(text); status.textContent = text; },
    })
    .withApplicationArguments(...helixArguments())
    .create();
  status.textContent = '';
  await runtime.runMain();
} catch (error) {
  status.textContent = 'Helix: ' + error;
  console.error(error);
}
