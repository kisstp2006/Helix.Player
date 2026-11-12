#include <Helix/Player/Host/Player.h>

// The Linux and Web player.
int main(int argc, char** argv)
{
    helix::Array<helix::StringView> args;
    for (int i = 1; i < argc; i++)
        args.Push(helix::StringView(argv[i]));
    return helix::player::RunPlayer(helix::Span<const helix::StringView>(args.Data(), args.Size()));
}
