/****************************************************************************
** ui.h extension file, included from the uic-generated form implementation.
**
** If you want to add, delete, or rename functions or slots, use
** Qt Designer to update this file, preserving your code.
**
** You should not define a constructor or destructor in this file.
** Instead, write your code in functions called init() and destroy().
** These will automatically be called by the form's constructor and
** destructor.
*****************************************************************************/

void SettingsDialog::init()
{
  QStringList sizes;
  
  sizes << i18n("unlimited")
        << i18n("650 MB CD")
        << i18n("700 MB CD")
        << i18n("4.7 GB DVD")
        << i18n("8.5 GB DVD")
        << i18n("9.4 GB DVD")
        << i18n("17.1 GB DVD")
        << i18n("custom");

  
  predefSizes->insertStringList(sizes);
  
  // unlimited as default
  predefSizes->setCurrentItem(0);
  maxSliceSize->setValue(0);
  maxSliceSize->setDisabled(true);
}

void SettingsDialog::sizeSelected( int idx )
{
  if ( idx == (predefSizes->count() - 1) )
    maxSliceSize->setEnabled(true);
  else
    maxSliceSize->setDisabled(true);
  
  // http://en.wikipedia.org/wiki/Binary_prefix#Binary_prefixes_using_SI_symbols
  // CD capacities are always given in binary units
  // But DVD capacities are given in decimal units

  switch ( idx )
  {
    // unlimited
    case 0: maxSliceSize->setValue(    0); break;

            // CDs
    case 1: maxSliceSize->setValue(  650); break;
    case 2: maxSliceSize->setValue(  700); break;

            // DVDs
    case 3: maxSliceSize->setValue( 4482); break;
    case 4: maxSliceSize->setValue( 8106); break;
    case 5: maxSliceSize->setValue( 8964); break;
    case 6: maxSliceSize->setValue(16307); break;

            // 700 MB CD
    case 7: maxSliceSize->setValue(  700); break;
  }
}


void SettingsDialog::setMaxMB( int mb )
{
  maxSliceSize->setValue(mb);
  maxSliceSize->setDisabled(true);
  
  switch ( mb )
  {
    case     0: predefSizes->setCurrentItem(0); break;
    case   650: predefSizes->setCurrentItem(1); break;
    case   700: predefSizes->setCurrentItem(2); break;
    case  4482: predefSizes->setCurrentItem(3); break;
    case  8106: predefSizes->setCurrentItem(4); break;
    case  8964: predefSizes->setCurrentItem(5); break;
    case 16307: predefSizes->setCurrentItem(6); break;

    default   : predefSizes->setCurrentItem(7);
                maxSliceSize->setEnabled(true);
                break;
  }
}
