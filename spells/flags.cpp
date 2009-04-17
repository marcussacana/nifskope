#include "../spellbook.h"

#include <QDialog>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QLayout>
#include <QPushButton>
#include <QSpinBox>

class spEditFlags : public Spell
{
public:
	QString name() const { return Spell::tr( "Flags" ); }
	bool instant() const { return true; }
	QIcon icon() const { return  QIcon( ":/img/flag" ); }
	
	enum FlagType
	{
		Alpha, Billboard, Controller, Node, RigidBody, Shape, Stencil, VertexColor, ZBuffer, BSX, None
	};
	
	QModelIndex getFlagIndex( const NifModel * nif, const QModelIndex & index ) const
	{
		if ( nif->itemName( index ) == "Flags" && nif->isNiBlock( index.parent() ) )
			return index;
		if ( nif->isNiBlock( index ) )
			return nif->getIndex( index, "Flags" );
		if ( nif->inherits( nif->getBlock( index ), "bhkRigidBody" ) )
		{
			QModelIndex iFlags = nif->getIndex( nif->getBlock( index ), "Col Filter" );
			iFlags = iFlags.sibling( iFlags.row(), NifModel::ValueCol );
			if ( index == iFlags )
				return iFlags;
		}
		else if ( nif->inherits( nif->getBlock( index ), "BSXFlags" ) )
		{
			QModelIndex iFlags = nif->getIndex( nif->getBlock( index ), "Integer Data" );
			iFlags = iFlags.sibling( iFlags.row(), NifModel::ValueCol );
			if ( index == iFlags )
				return iFlags;
		}
		return QModelIndex();
	}
	
	FlagType queryType( const NifModel * nif, const QModelIndex & index ) const
	{
		if ( nif->getValue( index ).isCount() )
		{
			QString name = nif->itemName( index.parent() );
			if ( name == "NiAlphaProperty" )
				return Alpha;
			else if ( name == "NiBillboardNode" )
				return Billboard;
			else if ( nif->inherits( name, "NiTimeController" ) )
				return Controller;
			else if ( name == "NiNode" )
				return Node;
			else if ( name == "bhkRigidBody" || name == "bhkRigidBodyT" )
				return RigidBody;
			else if ( name == "NiTriShape" || name == "NiTriStrips" )
				return Shape;
			else if ( name == "NiStencilProperty" )
				return Stencil;
			else if ( name == "NiVertexColorProperty" )
				return VertexColor;
			else if ( name == "NiZBufferProperty" )
				return ZBuffer;
			else if ( name == "BSXFlags" )
				return BSX;
		}
		return None;
	}
	
	bool isApplicable( const NifModel * nif, const QModelIndex & index )
	{
		return queryType( nif, getFlagIndex( nif, index ) ) != None;
	}
	
	QModelIndex cast( NifModel * nif, const QModelIndex & index )
	{
		QModelIndex iFlags = getFlagIndex( nif, index );
		
		switch ( queryType( nif, iFlags ) )
		{
			case Alpha:
				alphaFlags( nif, iFlags );
				break;
			case Billboard:
				billboardFlags( nif, iFlags);
				break;
			case Controller:
				controllerFlags( nif, iFlags );
				break;
			case Node:
				nodeFlags( nif, iFlags );
				break;
			case RigidBody:
				bodyFlags( nif, iFlags );
				break;
			case Shape:
				shapeFlags( nif, iFlags );
				break;
			case Stencil:
				stencilFlags( nif, iFlags );
				break;
			case VertexColor:
				vertexColorFlags( nif, iFlags );
				break;
			case ZBuffer:
				zbufferFlags( nif, iFlags );
				break;
			case BSX:
				bsxFlags(  nif, iFlags );
				break;
			default:
				break;
		}
		return index;
	}
	
	void alphaFlags( NifModel * nif, const QModelIndex & index )
	{
		quint16 flags = nif->get<int>( index );
		if ( ! flags ) flags = 0xed;
		
		QDialog dlg;
		QVBoxLayout * vbox = new QVBoxLayout;
		dlg.setLayout( vbox );
		
		// See glBlendFunc; ONE and ZERO appear to swapped from where they should be (not to mention missing values before SATURATE)
		QStringList blendModes = QStringList() <<
			"One" << "Zero" << "Src Color" << "Inv Src Color" << "Dst Color" << "Inv Dst Color" << "Src Alpha" << "Inv Src Alpha" <<
			"Dst Alpha" << "Inv Dst Alpha" << "Src Alpha Saturate";
		// ALWAYS and NEVER are swapped from where they should be; see glAlphaFunc
		QStringList testModes = QStringList() <<
			"Always" << "Less" << "Equal" << "Less or Equal" << "Greater" << "Not Equal" << "Greater or Equal" << "Never";
		
		QCheckBox * chkBlend = dlgCheck( vbox, "Enable Blending" );
		chkBlend->setChecked( flags & 1 );
		
		QComboBox * cmbSrc = dlgCombo( vbox, "Source Blend Mode", blendModes, chkBlend );
		cmbSrc->setCurrentIndex( flags >> 1 & 0x0f );
		
		QComboBox * cmbDst = dlgCombo( vbox, "Destination Blend Mode", blendModes, chkBlend );
		cmbDst->setCurrentIndex( flags >> 5 & 0x0f );
		
		QCheckBox * chkTest = dlgCheck( vbox, "Enable Testing" );
		chkTest->setChecked( flags & ( 1 << 9 ) );
		
		QComboBox * cmbTest = dlgCombo( vbox, "Alpha Test Function", testModes, chkTest );
		cmbTest->setCurrentIndex( flags >> 10 & 0x07 );
		
		QSpinBox * spnTest = dlgSpin( vbox, "Alpha Test Threshold", 0x00, 0xff, chkTest );
		spnTest->setValue( nif->get<int>( nif->getBlock( index ), "Threshold" ) );
		
		QCheckBox * chkSort = dlgCheck( vbox, "No Sorter" );
		chkSort->setChecked( ( flags & 0x2000 ) != 0 );
		
		dlgButtons( &dlg, vbox );
		
		if ( dlg.exec() == QDialog::Accepted )
		{
			flags = flags & 0xfffe;
			if ( chkBlend->isChecked() )
			{
				flags |= 1;
				flags = flags & 0xffe1 | cmbSrc->currentIndex() << 1;
				flags = flags & 0xfe1f | cmbDst->currentIndex() << 5;
			}
			
			flags = flags & 0xe1ff;
			if ( chkTest->isChecked() )
			{
				flags |= 0x0200;
				flags = flags & 0xe3ff | ( cmbTest->currentIndex() << 10 );
				nif->set<int>( nif->getBlock( index ), "Threshold", spnTest->value() );
			}
			
			flags = flags & 0xdfff | ( chkSort->isChecked() ? 0x2000 : 0 );
			
			nif->set<int>( index, flags );
		}
	}
	
	void nodeFlags( NifModel * nif, const QModelIndex & index )
	{
		quint16 flags = nif->get<int>( index );
		
		QDialog dlg;
		QVBoxLayout * vbox = new QVBoxLayout;
		dlg.setLayout( vbox );
		
		QCheckBox * chkHidden = dlgCheck( vbox, "Hidden" );
		chkHidden->setChecked( flags & 1 );
		
		QComboBox * cmbCollision = dlgCombo( vbox, "Collision Detection", QStringList() << "None" << "Triangles" << "Bounding Box" << "Continue" );
		cmbCollision->setCurrentIndex( flags >> 1 & 3 );
		
		QCheckBox * chkSkin = dlgCheck( vbox, "Skin Influence" );
		chkSkin->setChecked( ! ( flags & 8 ) );
		
		dlgButtons( &dlg, vbox );
		
		if ( dlg.exec() == QDialog::Accepted )
		{
			flags = flags & 0xfffe | ( chkHidden->isChecked() ? 1 : 0 );
			flags = flags & 0xfff9 | ( cmbCollision->currentIndex() << 1 );
			flags = flags & 0xfff7 | ( chkSkin->isChecked() ? 0 : 8 );
			nif->set<int>( index, flags );
		}
	}
	
	void controllerFlags( NifModel * nif, const QModelIndex & index )
	{
		quint16 flags = nif->get<int>( index );
		
		QDialog dlg;
		QVBoxLayout * vbox = new QVBoxLayout;
		dlg.setLayout( vbox );
		
		QCheckBox * chkActive = dlgCheck( vbox, "Active" );
		chkActive->setChecked( flags & 8 );
		
		QComboBox * cmbLoop = dlgCombo( vbox, "Loop Mode", QStringList() << "Cycle" << "Reverse" << "Clamp" );
		cmbLoop->setCurrentIndex( flags >> 1 & 3 );
		
		dlgButtons( &dlg, vbox );
		
		if ( dlg.exec() == QDialog::Accepted )
		{
			flags = flags & 0xfff7 | ( chkActive->isChecked() ? 8 : 0 );
			flags = flags & 0xfff9 | cmbLoop->currentIndex() << 1;
			nif->set<int>( index, flags );
		}
	}
	
	void bodyFlags( NifModel * nif, const QModelIndex & index )
	{
		quint16 flags = nif->get<int>( index );
		
		QDialog dlg;
		QVBoxLayout * vbox = new QVBoxLayout( &dlg );
		
		QCheckBox * chkLinked = dlgCheck( vbox, "Linked" );
		chkLinked->setChecked( flags & 0x80 );
		QCheckBox * chkNoCol  = dlgCheck( vbox, "No Collision" );
		chkNoCol->setChecked( flags & 0x40 );
		QCheckBox * chkScaled = dlgCheck( vbox, "Scaled" );
		chkScaled->setChecked( flags & 0x20 );
		
		QSpinBox * spnPartNo = dlgSpin( vbox, "Part Number", 0, 0x1f, chkLinked );
		spnPartNo->setValue( flags & 0x1f );
		
		dlgButtons( &dlg, vbox );
		
		if ( dlg.exec() == QDialog::Accepted )
		{
			flags = flags & 0x7f | ( chkLinked->isChecked() ? 0x80 : 0 );
			flags = flags & 0xbf | ( chkNoCol->isChecked() ? 0x40 : 0 );
			flags = flags & 0xdf | ( chkScaled->isChecked() ? 0x20 : 0 );
			flags = flags & 0xe0 | ( chkLinked->isChecked() ? spnPartNo->value() : 0 );
			nif->set<int>( index, flags );
			nif->set<int>( index.parent(), "Col Filter Copy", flags );
		}
	}
	
	void shapeFlags( NifModel * nif, const QModelIndex & index )
	{
		quint16 flags = nif->get<int>( index );
		
		QDialog dlg;
		QVBoxLayout * vbox = new QVBoxLayout;
		dlg.setLayout( vbox );
		
		QCheckBox * chkHidden = dlgCheck( vbox, "Hidden" );
		chkHidden->setChecked( flags & 0x01 );
		
		QComboBox * cmbCollision = dlgCombo( vbox, "Collision Detection", QStringList() << "None" << "Triangles" << "Bounding Box" << "Continue" );
		cmbCollision->setCurrentIndex( flags >> 1 & 3 );
		
		QCheckBox * chkShadow = 0;
		if ( nif->checkVersion( 0x04000002, 0x04000002 ) )
		{
			chkShadow = dlgCheck( vbox, "Shadow" );
			chkShadow->setChecked( flags & 0x40 );
		}
		
		dlgButtons( &dlg, vbox );
		
		if ( dlg.exec() == QDialog::Accepted )
		{
			flags = flags & 0xfffe | ( chkHidden->isChecked() ? 0x01 : 0 );
			flags = flags & 0xfff9 | ( cmbCollision->currentIndex() << 1 );
			if ( chkShadow )
				flags = flags & 0xffbf | ( chkShadow->isChecked() ? 0x40 : 0 );
			nif->set<int>( index, flags );
		}
	}
	
	void zbufferFlags( NifModel * nif, const QModelIndex & index )
	{
		quint16 flags = nif->get<int>( index );
		
		QDialog dlg;
		QVBoxLayout * vbox = new QVBoxLayout;
		dlg.setLayout( vbox );
		
		QCheckBox * chkEnable = dlgCheck( vbox, "Enable Z Buffer" );
		chkEnable->setChecked( flags & 1 );
		
		QCheckBox * chkROnly = dlgCheck( vbox, "Z Buffer Read Only" );
		chkROnly->setChecked( ( flags & 2 ) == 0 );
		
		// NetImmerse/Gamebyro swapped ALWAYS and NEVER here, too. Otherwise values match glDepthFunc 
		QStringList compareFunc = QStringList()
			<< Spell::tr("Always") // 0
			<< Spell::tr("Less") // 1
			<< Spell::tr("Equal") // 2
			<< Spell::tr("Less or Equal") // 3
			<< Spell::tr("Greater") // 4
			<< Spell::tr("Not Equal") // 5
			<< Spell::tr("Greater or Equal") // 6
			<< Spell::tr("Never"); // 7
		
		QComboBox * cmbFunc = dlgCombo( vbox, Spell::tr("Z Buffer Test Function"), compareFunc, chkEnable );
		if ( nif->checkVersion( 0x04010012, 0x14000005 ) )
			cmbFunc->setCurrentIndex( nif->get<int>( nif->getBlock( index ), "Function" ) );
		else
			cmbFunc->setCurrentIndex( ( flags >> 2 ) & 0x07 );
		
		dlgButtons( &dlg, vbox );
		
		// TODO: check which is used for 4.1.0.12 < x < 20.0.0.5 if flags
		// and function conflict... perhaps set flags regardless?
		if ( dlg.exec() == QDialog::Accepted )
		{
			flags = flags & 0xfffe | ( chkEnable->isChecked() ? 1 : 0 );
			flags = flags & 0xfffd | ( chkROnly->isChecked() ? 0 : 2 );
			if ( nif->checkVersion( 0x0401000C, 0x14000005 ) )
				nif->set<int>( nif->getBlock( index ), "Function", cmbFunc->currentIndex() );
			else
				flags = flags & 0xffe3 | ( cmbFunc->currentIndex() << 2 );
			nif->set<int>( index, flags );
		}
	}
	
	void bsxFlags( NifModel * nif, const QModelIndex & index )
	{
		quint32 flags = nif->get<int>( index );
		
		QDialog dlg;
		QVBoxLayout * vbox = new QVBoxLayout( & dlg );
		
		QStringList flagNames( QStringList()
			<< Spell::tr( "Enable Animation" ) // 1
			<< Spell::tr( "Enable Collision" ) // 2
			<< Spell::tr( "Is Skeleton Nif (?)" ) // 4
			<< Spell::tr( "Unidentified Flag (?)" ) // 8
			<< Spell::tr( "FlameNodes Present" ) // 16
			<< Spell::tr( "EditorMarkers Present" ) // 32
		);
		
		QList<QCheckBox *> chkBoxes;
		int x = 0;
		foreach ( QString flagName, flagNames )
		{
			chkBoxes << dlgCheck( vbox, QString( "%1 (%2)" ).arg( flagName ).arg( 1 << x ) );
			chkBoxes.last()->setChecked( flags & ( 1 << x ) );
			x++;
		}
		
		dlgButtons( &dlg, vbox );
		
		if ( dlg.exec() == QDialog::Accepted )
		{
			x = 0;
			foreach ( QCheckBox * chk, chkBoxes )
			{
				flags = flags & ( ~ ( 1 << x ) ) | ( chk->isChecked() ? 1 << x : 0 );
				x++;
			}
			nif->set<int>( index, flags );
		}
	}
	
	void billboardFlags( NifModel * nif, const QModelIndex & index )
	{
		quint16 flags = nif->get<int>( index );
		if ( ! flags ) flags = 0x8;
		
		QDialog dlg;
		QVBoxLayout * vbox = new QVBoxLayout;
		dlg.setLayout( vbox );
		
		QCheckBox * chkHidden = dlgCheck( vbox, Spell::tr("Hidden") );
		chkHidden->setChecked( flags & 1 );
		
		QStringList collideModes = QStringList()
			<< Spell::tr("None") // 0
			<< Spell::tr("Triangles") // 2
			<< Spell::tr("Bounding Box") // 4
			<< Spell::tr("Continue"); // 6
		QStringList billboardModes = QStringList()
			<< Spell::tr("Always Face Camera") // 0
			<< Spell::tr("Rotate About Up") // 32
			<< Spell::tr("Rigid Face Camera") // 64
			<< Spell::tr("Always Face Center"); // 96
		
		QComboBox * cmbCollision = dlgCombo( vbox, Spell::tr("Collision Detection"), collideModes );
		cmbCollision->setCurrentIndex( flags >> 1 & 3 );
		
		QComboBox * cmbMode = dlgCombo( vbox, Spell::tr("Billboard Mode"), billboardModes );
		cmbMode->setCurrentIndex( flags >> 5 & 3 );
		
		dlgButtons( &dlg, vbox );
		
		if ( dlg.exec() == QDialog::Accepted )
		{
			flags = flags & 0xfffe | ( chkHidden->isChecked() ? 1 : 0 );
			flags = flags & 0xfff9 | ( cmbCollision->currentIndex() << 1);
			flags = flags & 0xff9f | ( cmbMode->currentIndex() << 5 );
			flags = flags & 0xfff7 | 8; // seems to always be set but has no known effect
			nif->set<int>( index, flags );
		}
	}
	
	void stencilFlags( NifModel * nif, const QModelIndex & index )
	{
		quint16 flags = nif->get<int>( index );
		
		QDialog dlg;
		QVBoxLayout * vbox = new QVBoxLayout;
		dlg.setLayout( vbox );
		
		QCheckBox * chkEnable = dlgCheck( vbox, Spell::tr("Stencil Enable") );
		
		QStringList stencilActions = QStringList()
			<< Spell::tr("Keep") // 0
			<< Spell::tr("Zero") // 1
			<< Spell::tr("Replace") // 2
			<< Spell::tr("Increment") // 3
			<< Spell::tr("Decrement") // 4
			<< Spell::tr("Invert"); // 5
		
		QComboBox * cmbFail = dlgCombo( vbox, Spell::tr("Fail Action"), stencilActions );
		
		QComboBox * cmbZFail = dlgCombo( vbox, Spell::tr("Z Fail Action"), stencilActions );
		
		QComboBox * cmbPass = dlgCombo( vbox, Spell::tr("Pass Action"), stencilActions );
		
		QStringList drawModes = QStringList()
			<< Spell::tr("Counter clock wise or Both") // 0
			<< Spell::tr("Draw counter clock wise") // 1
			<< Spell::tr("Draw clock wise") // 2
			<< Spell::tr("Draw Both"); // 3
		
		QComboBox * cmbDrawMode = dlgCombo( vbox, Spell::tr("Draw Mode"), drawModes );
		
		// Appears to match glStencilFunc
		QStringList compareFunc = QStringList()
			<< Spell::tr("Never") // 0
			<< Spell::tr("Less") // 1
			<< Spell::tr("Equal") // 2
			<< Spell::tr("Less or Equal") // 3
			<< Spell::tr("Greater") // 4
			<< Spell::tr("Not Equal") // 5
			<< Spell::tr("Greater or Equal") // 6
			<< Spell::tr("Always"); // 7
		
		QComboBox * cmbFunc = dlgCombo( vbox, Spell::tr("Stencil Function"), compareFunc );
		
		// prior to 20.1.0.3 flags itself appears unused; between 10.0.1.2 and 20.1.0.3 it is not present
		// 20.0.0.5 is the last version with these values
		if ( nif->checkVersion( 0, 0x14000005 ) )
		{
			// set based on Stencil Enabled, Stencil Function, Fail Action, Z Fail Action, Pass Action, Draw Mode
			// Possibly include Stencil Ref and Stencil Mask except they don't seem to ever vary from the default
			chkEnable->setChecked( nif->get<bool>( nif->getBlock( index ), "Stencil Enabled" ) );
			cmbFail->setCurrentIndex( nif->get<int>( nif->getBlock( index ), "Fail Action" ) );
			cmbZFail->setCurrentIndex( nif->get<int>( nif->getBlock( index ), "Z Fail Action" ) );
			cmbPass->setCurrentIndex( nif->get<int>( nif->getBlock( index ), "Pass Action" ) );
			cmbDrawMode->setCurrentIndex( nif->get<int>( nif->getBlock( index ), "Draw Mode" ) );
			cmbFunc->setCurrentIndex( nif->get<int>( nif->getBlock( index ), "Stencil Function" ) );
		}
		else
		{
			// set based on flags itself
			chkEnable->setChecked( flags & 1 );
			cmbFail->setCurrentIndex( flags >> 1 & 7 );
			cmbZFail->setCurrentIndex( flags >> 4 & 7 );
			cmbPass->setCurrentIndex( flags >> 7 & 7 );
			cmbDrawMode->setCurrentIndex( flags >> 10 & 3 );
			cmbFunc->setCurrentIndex( flags >> 12 & 7 );
		}
		
		dlgButtons( &dlg, vbox );
		
		if ( dlg.exec() && QDialog::Accepted )
		{
			if ( nif->checkVersion( 0, 0x14000005 ) )
			{
				nif->set<bool>( nif->getBlock( index ), "Stencil Enabled", chkEnable->isChecked() );
				nif->set<int>( nif->getBlock( index ), "Fail Action", cmbFail->currentIndex() );
				nif->set<int>( nif->getBlock( index ), "Z Fail Action", cmbZFail->currentIndex() );
				nif->set<int>( nif->getBlock( index ), "Pass Action", cmbPass->currentIndex() );
				nif->set<int>( nif->getBlock( index ), "Draw Mode", cmbDrawMode->currentIndex() );
				nif->set<int>( nif->getBlock( index ), "Stencil Function", cmbFunc->currentIndex() );
			}
			else
			{
				flags = flags & 0xfffe | ( chkEnable->isChecked() ? 1 : 0 );
				flags = flags & 0xfff1 | ( cmbFail->currentIndex() << 1 );
				flags = flags & 0xff8f | ( cmbZFail->currentIndex() << 4 );
				flags = flags & 0xfc7f | ( cmbPass->currentIndex() << 7 );
				flags = flags & 0xf3ff | ( cmbDrawMode->currentIndex() << 10 );
				flags = flags & 0xcfff | ( cmbFunc->currentIndex() << 12 );
				nif->set<int>( index, flags );
			}
		}
	}
	
	void vertexColorFlags( NifModel * nif, const QModelIndex & index )
	{
		quint16 flags = nif->get<int>( index );
		
		QDialog dlg;
		QVBoxLayout * vbox = new QVBoxLayout;
		dlg.setLayout( vbox );
		
		// For versions < 20.1.0.3 (<= 20.0.0.5), give option to set flags regardless
		QCheckBox * setFlags = 0;
		if( nif->checkVersion( 0, 0x14000005 ) )
		{
			setFlags = dlgCheck( vbox, Spell::tr("Set Flags also") );
			setFlags->setChecked( flags != 0 );
		}
		
		QStringList lightMode = QStringList()
			<< Spell::tr("Emissive")
			<< Spell::tr("Emissive + Ambient + Diffuse");
		
		QComboBox * cmbLight = dlgCombo( vbox, Spell::tr("Lighting Mode"), lightMode );
		
		QStringList vertMode = QStringList()
			<< Spell::tr("Source Ignore")
			<< Spell::tr("Source Emissive")
			<< Spell::tr("Source Ambient/Diffuse");
		
		QComboBox * cmbVert = dlgCombo( vbox, Spell::tr("Vertex Mode"), vertMode );
		
		// Use enums in preference to flags since they probably have a higher priority
		if ( nif->checkVersion( 0, 0x14000005 ) )
		{
			cmbLight->setCurrentIndex( nif->get<int>( nif->getBlock( index ), "Lighting Mode" ) );
			cmbVert->setCurrentIndex( nif->get<int>( nif->getBlock( index ), "Vertex Mode" ) );
		}
		else
		{
			cmbLight->setCurrentIndex( flags >> 3 & 1 );
			cmbVert->setCurrentIndex( flags >> 4 & 3 );
		}
		
		dlgButtons( &dlg, vbox );
		
		if ( dlg.exec() && QDialog::Accepted )
		{
			if ( nif->checkVersion( 0, 0x14000005 ) )
			{
				nif->set<int>( nif->getBlock( index ), "Lighting Mode", cmbLight->currentIndex() );
				nif->set<int>( nif->getBlock( index ), "Vertex Mode", cmbVert->currentIndex() );
			}
			
			if( nif->checkVersion( 0x14010003, 0 ) || setFlags != 0 && setFlags->isChecked() )
			{
				flags = flags & 0xfff7 | ( cmbLight->currentIndex() << 3 );
				flags = flags & 0xffcf | ( cmbVert->currentIndex() << 4 );
				nif->set<int>( index, flags );
			}
		}
	}
	
	QCheckBox * dlgCheck( QVBoxLayout * vbox, const QString & name, QCheckBox * chk = 0 )
	{
		QCheckBox * box = new QCheckBox( name );
		vbox->addWidget( box );
		if ( chk )
		{
			QObject::connect( chk, SIGNAL( toggled( bool ) ), box, SLOT( setEnabled( bool ) ) );
			box->setEnabled( chk->isChecked() );
		}
		return box;
	}
	
	QComboBox * dlgCombo( QVBoxLayout * vbox, const QString & name, QStringList items, QCheckBox * chk = 0 )
	{
		vbox->addWidget( new QLabel( name ) );
		QComboBox * cmb = new QComboBox;
		vbox->addWidget( cmb );
		cmb->addItems( items );
		if ( chk )
		{
			QObject::connect( chk, SIGNAL( toggled( bool ) ), cmb, SLOT( setEnabled( bool ) ) );
			cmb->setEnabled( chk->isChecked() );
		}
		return cmb;
	}
	
	QSpinBox * dlgSpin( QVBoxLayout * vbox, const QString & name, int min, int max, QCheckBox * chk = 0 )
	{
		vbox->addWidget( new QLabel( name ) );
		QSpinBox * spn = new QSpinBox;
		vbox->addWidget( spn );
		spn->setRange( min, max );
		if ( chk )
		{
			QObject::connect( chk, SIGNAL( toggled( bool ) ), spn, SLOT( setEnabled( bool ) ) );
			spn->setEnabled( chk->isChecked() );
		}
		return spn;
	}
	
	void dlgButtons( QDialog * dlg, QVBoxLayout * vbox )
	{
		QHBoxLayout * hbox = new QHBoxLayout;
		vbox->addLayout( hbox );
		
		QPushButton * btAccept = new QPushButton( "Accept" );
		hbox->addWidget( btAccept );
		QObject::connect( btAccept, SIGNAL( clicked() ), dlg, SLOT( accept() ) );
		
		QPushButton * btReject = new QPushButton( "Cancel" );
		hbox->addWidget( btReject );
		QObject::connect( btReject, SIGNAL( clicked() ), dlg, SLOT( reject() ) );
	}
};

REGISTER_SPELL( spEditFlags )
