#pragma once
#include <wx/grid.h>
namespace grid {
    inline bool fnMoveDown(wxGrid *grid, int col) {
        if (grid->GetGridCursorRow() == grid->GetNumberRows() - 1) {
            grid->AppendRows();
        }
        grid->GoToCell(grid->GetGridCursorRow() + 1, col);
        return true;
    }

    inline bool fnMoveRight(wxGrid *grid) {
        if (!grid->MoveCursorRight(false)) {
            return fnMoveDown(grid, 0);
        }
        return false;
    }
}