
export CAKE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/tools/cake/src/run.py"

function cake()
{
  python2.7 "$CAKE" "$@"
}
