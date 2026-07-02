param(
    [string]$OutputDirectory_ = "Data\Model\Stage"
)

$ErrorActionPreference = "Stop"
$segments_ = 64
$floorHeight_ = -0.0159488618
$borderTopHeight_ = -0.008
$outerFloorHeight_ = -0.017
$trimTopHeight_ = -0.006
$trimBottomHeight_ = -0.060
$centerRadius_ = 0.53
$borderOuterRadius_ = 0.59
$trimInnerRadius_ = 0.90
$outerRadius_ = 1.0

function New-VertexData
{
    return [ordered]@{
        Positions = [System.Collections.Generic.List[float]]::new()
        Normals = [System.Collections.Generic.List[float]]::new()
        Tangents = [System.Collections.Generic.List[float]]::new()
        Texcoords = [System.Collections.Generic.List[float]]::new()
        Indices = [System.Collections.Generic.List[uint32]]::new()
    }
}

function Add-Vertex($data_, $x_, $y_, $z_, $normalX_, $normalY_, $normalZ_, $u_, $v_)
{
    $data_.Positions.Add([float]$x_)
    $data_.Positions.Add([float]$y_)
    $data_.Positions.Add([float]$z_)
    $data_.Normals.Add([float]$normalX_)
    $data_.Normals.Add([float]$normalY_)
    $data_.Normals.Add([float]$normalZ_)
    $tangentX_ = 1.0 - $normalX_ * $normalX_
    $tangentY_ = -$normalX_ * $normalY_
    $tangentZ_ = -$normalX_ * $normalZ_
    $tangentLength_ = [Math]::Sqrt($tangentX_ * $tangentX_ + $tangentY_ * $tangentY_ + $tangentZ_ * $tangentZ_)
    if ($tangentLength_ -lt 0.0001)
    {
        $tangentX_ = 0.0
        $tangentY_ = 0.0
        $tangentZ_ = 1.0
        $tangentLength_ = 1.0
    }
    $data_.Tangents.Add([float]($tangentX_ / $tangentLength_))
    $data_.Tangents.Add([float]($tangentY_ / $tangentLength_))
    $data_.Tangents.Add([float]($tangentZ_ / $tangentLength_))
    $data_.Tangents.Add(1.0)
    $data_.Texcoords.Add([float]$u_)
    $data_.Texcoords.Add([float]$v_)
}

function Add-TopVertex($data_, $radius_, $height_, $angle_)
{
    $x_ = [Math]::Cos($angle_) * $radius_
    $z_ = [Math]::Sin($angle_) * $radius_
    Add-Vertex $data_ $x_ $height_ $z_ 0.0 1.0 0.0 (0.5 + $x_ * 0.5) (0.5 - $z_ * 0.5)
}

function New-CenterMesh
{
    $data_ = New-VertexData
    Add-Vertex $data_ 0.0 $floorHeight_ 0.0 0.0 1.0 0.0 0.5 0.5
    for ($segmentIndex_ = 0; $segmentIndex_ -le $segments_; ++$segmentIndex_)
    {
        $angle_ = 2.0 * [Math]::PI * $segmentIndex_ / $segments_
        Add-TopVertex $data_ $centerRadius_ $floorHeight_ $angle_
    }
    for ($segmentIndex_ = 0; $segmentIndex_ -lt $segments_; ++$segmentIndex_)
    {
        $data_.Indices.Add(0)
        $data_.Indices.Add([uint32]($segmentIndex_ + 2))
        $data_.Indices.Add([uint32]($segmentIndex_ + 1))
    }
    return $data_
}

function Add-TopAnnulus($data_, $innerRadius_, $outerRadius_, $height_)
{
    $baseVertex_ = [uint32]($data_.Positions.Count / 3)
    for ($segmentIndex_ = 0; $segmentIndex_ -le $segments_; ++$segmentIndex_)
    {
        $angle_ = 2.0 * [Math]::PI * $segmentIndex_ / $segments_
        Add-TopVertex $data_ $innerRadius_ $height_ $angle_
        Add-TopVertex $data_ $outerRadius_ $height_ $angle_
    }
    for ($segmentIndex_ = 0; $segmentIndex_ -lt $segments_; ++$segmentIndex_)
    {
        $innerCurrent_ = $baseVertex_ + [uint32]($segmentIndex_ * 2)
        $outerCurrent_ = $innerCurrent_ + 1
        $innerNext_ = $innerCurrent_ + 2
        $outerNext_ = $innerCurrent_ + 3
        $data_.Indices.Add($innerCurrent_)
        $data_.Indices.Add($innerNext_)
        $data_.Indices.Add($outerNext_)
        $data_.Indices.Add($innerCurrent_)
        $data_.Indices.Add($outerNext_)
        $data_.Indices.Add($outerCurrent_)
    }
}

function Add-BeveledBorderRing($data_, $innerRadius_, $outerRadius_, $innerHeight_, $topHeight_, $outerHeight_)
{
    $bevelWidth_ = ($outerRadius_ - $innerRadius_) * 0.18
    $innerTopRadius_ = $innerRadius_ + $bevelWidth_
    $outerTopRadius_ = $outerRadius_ - $bevelWidth_
    $innerSlope_ = ($topHeight_ - $innerHeight_) / $bevelWidth_
    $outerSlope_ = ($outerHeight_ - $topHeight_) / $bevelWidth_
    $innerNormalLength_ = [Math]::Sqrt($innerSlope_ * $innerSlope_ + 1.0)
    $outerNormalLength_ = [Math]::Sqrt($outerSlope_ * $outerSlope_ + 1.0)
    $baseVertex_ = [uint32]($data_.Positions.Count / 3)

    for ($segmentIndex_ = 0; $segmentIndex_ -le $segments_; ++$segmentIndex_)
    {
        $angle_ = 2.0 * [Math]::PI * $segmentIndex_ / $segments_
        $radialX_ = [Math]::Cos($angle_)
        $radialZ_ = [Math]::Sin($angle_)
        $innerNormalX_ = -$innerSlope_ * $radialX_ / $innerNormalLength_
        $innerNormalY_ = 1.0 / $innerNormalLength_
        $innerNormalZ_ = -$innerSlope_ * $radialZ_ / $innerNormalLength_
        $outerNormalX_ = -$outerSlope_ * $radialX_ / $outerNormalLength_
        $outerNormalY_ = 1.0 / $outerNormalLength_
        $outerNormalZ_ = -$outerSlope_ * $radialZ_ / $outerNormalLength_

        $rows_ = @(
            @($innerRadius_, $innerHeight_, $innerNormalX_, $innerNormalY_, $innerNormalZ_),
            @($innerTopRadius_, $topHeight_, $innerNormalX_, $innerNormalY_, $innerNormalZ_),
            @($innerTopRadius_, $topHeight_, 0.0, 1.0, 0.0),
            @($outerTopRadius_, $topHeight_, 0.0, 1.0, 0.0),
            @($outerTopRadius_, $topHeight_, $outerNormalX_, $outerNormalY_, $outerNormalZ_),
            @($outerRadius_, $outerHeight_, $outerNormalX_, $outerNormalY_, $outerNormalZ_)
        )
        foreach ($row_ in $rows_)
        {
            $x_ = $radialX_ * $row_[0]
            $z_ = $radialZ_ * $row_[0]
            Add-Vertex $data_ $x_ $row_[1] $z_ $row_[2] $row_[3] $row_[4] (0.5 + $x_ * 0.5) (0.5 - $z_ * 0.5)
        }
    }

    $surfacePairs_ = @(@(0, 1), @(2, 3), @(4, 5))
    for ($segmentIndex_ = 0; $segmentIndex_ -lt $segments_; ++$segmentIndex_)
    {
        foreach ($surfacePair_ in $surfacePairs_)
        {
            $currentInner_ = $baseVertex_ + [uint32]($segmentIndex_ * 6 + $surfacePair_[0])
            $currentOuter_ = $baseVertex_ + [uint32]($segmentIndex_ * 6 + $surfacePair_[1])
            $nextInner_ = $currentInner_ + 6
            $nextOuter_ = $currentOuter_ + 6
            $data_.Indices.Add($currentInner_)
            $data_.Indices.Add($nextInner_)
            $data_.Indices.Add($nextOuter_)
            $data_.Indices.Add($currentInner_)
            $data_.Indices.Add($nextOuter_)
            $data_.Indices.Add($currentOuter_)
        }
    }
}

function Add-RingSide($data_, $radius_, $topHeight_, $bottomHeight_, $normalDirection_)
{
    $baseVertex_ = [uint32]($data_.Positions.Count / 3)
    for ($segmentIndex_ = 0; $segmentIndex_ -le $segments_; ++$segmentIndex_)
    {
        $angle_ = 2.0 * [Math]::PI * $segmentIndex_ / $segments_
        $x_ = [Math]::Cos($angle_) * $radius_
        $z_ = [Math]::Sin($angle_) * $radius_
        $normalX_ = [Math]::Cos($angle_) * $normalDirection_
        $normalZ_ = [Math]::Sin($angle_) * $normalDirection_
        $u_ = $segmentIndex_ / [float]$segments_
        Add-Vertex $data_ $x_ $topHeight_ $z_ $normalX_ 0.0 $normalZ_ $u_ 0.0
        Add-Vertex $data_ $x_ $bottomHeight_ $z_ $normalX_ 0.0 $normalZ_ $u_ 1.0
    }
    for ($segmentIndex_ = 0; $segmentIndex_ -lt $segments_; ++$segmentIndex_)
    {
        $topCurrent_ = $baseVertex_ + [uint32]($segmentIndex_ * 2)
        $bottomCurrent_ = $topCurrent_ + 1
        $topNext_ = $topCurrent_ + 2
        $bottomNext_ = $topCurrent_ + 3
        if ($normalDirection_ -gt 0.0)
        {
            $data_.Indices.Add($topCurrent_)
            $data_.Indices.Add($bottomNext_)
            $data_.Indices.Add($topNext_)
            $data_.Indices.Add($topCurrent_)
            $data_.Indices.Add($bottomCurrent_)
            $data_.Indices.Add($bottomNext_)
        }
        else
        {
            $data_.Indices.Add($topCurrent_)
            $data_.Indices.Add($topNext_)
            $data_.Indices.Add($bottomNext_)
            $data_.Indices.Add($topCurrent_)
            $data_.Indices.Add($bottomNext_)
            $data_.Indices.Add($bottomCurrent_)
        }
    }
}

$centerMesh_ = New-CenterMesh
$borderMesh_ = New-VertexData
Add-BeveledBorderRing $borderMesh_ $centerRadius_ $borderOuterRadius_ $floorHeight_ $borderTopHeight_ $outerFloorHeight_
$tileMesh_ = New-VertexData
Add-TopAnnulus $tileMesh_ $borderOuterRadius_ $trimInnerRadius_ $outerFloorHeight_
$trimMesh_ = New-VertexData
Add-TopAnnulus $trimMesh_ $trimInnerRadius_ $outerRadius_ $trimTopHeight_
Add-RingSide $trimMesh_ $outerRadius_ $trimTopHeight_ $trimBottomHeight_ 1.0
Add-RingSide $trimMesh_ $trimInnerRadius_ $trimTopHeight_ $floorHeight_ -1.0

$bufferViews_ = [System.Collections.Generic.List[object]]::new()
$accessors_ = [System.Collections.Generic.List[object]]::new()
$meshes_ = [System.Collections.Generic.List[object]]::new()
$binaryPath_ = Join-Path $OutputDirectory_ "Stage_Floor05.bin"
$stream_ = [System.IO.File]::Open($binaryPath_, [System.IO.FileMode]::Create)
$writer_ = [System.IO.BinaryWriter]::new($stream_)

function Write-FloatList($values_)
{
    foreach ($value_ in $values_)
    {
        $writer_.Write([float]$value_)
    }
}

function Write-UIntList($values_)
{
    foreach ($value_ in $values_)
    {
        $writer_.Write([uint32]$value_)
    }
}

function Add-BufferData($values_, $componentType_, $type_, $target_, $count_, $minimum_, $maximum_)
{
    $offset_ = [int]$stream_.Position
    if ($componentType_ -eq 5126) { Write-FloatList $values_ } else { Write-UIntList $values_ }
    $length_ = [int]$stream_.Position - $offset_
    $bufferViewIndex_ = $bufferViews_.Count
    $bufferViews_.Add([ordered]@{ buffer = 0; byteOffset = $offset_; byteLength = $length_; target = $target_ })
    $accessor_ = [ordered]@{ bufferView = $bufferViewIndex_; componentType = $componentType_; count = $count_; type = $type_ }
    if ($null -ne $minimum_) { $accessor_.min = $minimum_ }
    if ($null -ne $maximum_) { $accessor_.max = $maximum_ }
    $accessorIndex_ = $accessors_.Count
    $accessors_.Add($accessor_)
    return $accessorIndex_
}

function Add-Mesh($name_, $data_, $materialIndex_, $minimum_, $maximum_)
{
    $vertexCount_ = [int]($data_.Positions.Count / 3)
    $positionAccessor_ = Add-BufferData $data_.Positions 5126 "VEC3" 34962 $vertexCount_ $minimum_ $maximum_
    $normalAccessor_ = Add-BufferData $data_.Normals 5126 "VEC3" 34962 $vertexCount_ $null $null
    $tangentAccessor_ = Add-BufferData $data_.Tangents 5126 "VEC4" 34962 $vertexCount_ $null $null
    $texcoordAccessor_ = Add-BufferData $data_.Texcoords 5126 "VEC2" 34962 $vertexCount_ $null $null
    $indexAccessor_ = Add-BufferData $data_.Indices 5125 "SCALAR" 34963 $data_.Indices.Count @(0) @($vertexCount_ - 1)
    $meshes_.Add([ordered]@{
        name = $name_
        primitives = @([ordered]@{
            attributes = [ordered]@{ POSITION = $positionAccessor_; NORMAL = $normalAccessor_; TANGENT = $tangentAccessor_; TEXCOORD_0 = $texcoordAccessor_ }
            indices = $indexAccessor_
            material = $materialIndex_
            mode = 4
        })
    })
}

Add-Mesh "Stage_Center_Circle" $centerMesh_ 0 @(-$centerRadius_, $floorHeight_, -$centerRadius_) @($centerRadius_, $floorHeight_, $centerRadius_)
Add-Mesh "Stage_Border_Ring" $borderMesh_ 1 @(-$borderOuterRadius_, $outerFloorHeight_, -$borderOuterRadius_) @($borderOuterRadius_, $borderTopHeight_, $borderOuterRadius_)
Add-Mesh "Stage_Outer_Tile_Floor" $tileMesh_ 2 @(-$trimInnerRadius_, $outerFloorHeight_, -$trimInnerRadius_) @($trimInnerRadius_, $outerFloorHeight_, $trimInnerRadius_)
Add-Mesh "Stage_Outer_Trim" $trimMesh_ 3 @(-$outerRadius_, $trimBottomHeight_, -$outerRadius_) @($outerRadius_, $trimTopHeight_, $outerRadius_)
$writer_.Dispose()
$stream_.Dispose()

$combinedPrimitives_ = @(
    $meshes_[0].primitives[0],
    $meshes_[1].primitives[0],
    $meshes_[2].primitives[0],
    $meshes_[3].primitives[0]
)

$gltf_ = [ordered]@{
    asset = [ordered]@{ version = "2.0"; generator = "BIGShine Stage Floor05 Parts Generator" }
    scene = 0
    scenes = @([ordered]@{ name = "Stage_Floor05"; nodes = @(0) })
    nodes = @([ordered]@{ name = "Stage_Floor05"; mesh = 0 })
    meshes = @([ordered]@{ name = "Stage_Floor05_Parts"; primitives = $combinedPrimitives_ })
    materials = @(
        [ordered]@{
            name = "Stage_Center_Circle"
            pbrMetallicRoughness = [ordered]@{ baseColorTexture = [ordered]@{ index = 0 }; metallicFactor = 0.0; roughnessFactor = 0.5 }
            normalTexture = [ordered]@{ index = 1 }
            occlusionTexture = [ordered]@{ index = 2 }
        },
        [ordered]@{
            name = "Stage_Border_Ring"
            pbrMetallicRoughness = [ordered]@{ baseColorTexture = [ordered]@{ index = 0 }; metallicFactor = 0.0; roughnessFactor = 0.72; baseColorFactor = @(0.58, 0.54, 0.50, 1.0) }
            normalTexture = [ordered]@{ index = 1; scale = 0.85 }
            occlusionTexture = [ordered]@{ index = 2; strength = 1.0 }
        },
        [ordered]@{
            name = "Stage_Outer_Tile_Floor"
            pbrMetallicRoughness = [ordered]@{ baseColorTexture = [ordered]@{ index = 3 }; metallicRoughnessTexture = [ordered]@{ index = 5 }; metallicFactor = 0.0; roughnessFactor = 0.85; baseColorFactor = @(0.42, 0.36, 0.32, 1.0) }
            normalTexture = [ordered]@{ index = 4; scale = 0.7 }
            occlusionTexture = [ordered]@{ index = 5; strength = 1.0 }
        },
        [ordered]@{
            name = "Stage_Outer_Trim"
            pbrMetallicRoughness = [ordered]@{ baseColorTexture = [ordered]@{ index = 0 }; metallicFactor = 0.0; roughnessFactor = 0.65; baseColorFactor = @(0.72, 0.68, 0.64, 1.0) }
            normalTexture = [ordered]@{ index = 1 }
            occlusionTexture = [ordered]@{ index = 2 }
        }
    )
    samplers = @(
        [ordered]@{ magFilter = 9729; minFilter = 9987; wrapS = 33071; wrapT = 33071 },
        [ordered]@{ magFilter = 9729; minFilter = 9987; wrapS = 10497; wrapT = 10497 }
    )
    images = @(
        [ordered]@{ uri = "FloorBase04.png" },
        [ordered]@{ uri = "Floor_Normal.png" },
        [ordered]@{ uri = "Floor_AO.png" },
        [ordered]@{ uri = "PavingStones111_4K-PNG_Color.png" },
        [ordered]@{ uri = "PavingStones111_4K-PNG_NormalGL.png.png" },
        [ordered]@{ uri = "PavingStones111_4K-PNG_Roughness.png.png" }
    )
    textures = @(
        [ordered]@{ sampler = 0; source = 0 },
        [ordered]@{ sampler = 0; source = 1 },
        [ordered]@{ sampler = 0; source = 2 },
        [ordered]@{ sampler = 1; source = 3 },
        [ordered]@{ sampler = 1; source = 4 },
        [ordered]@{ sampler = 1; source = 5 }
    )
    buffers = @([ordered]@{ uri = "Stage_Floor05.bin"; byteLength = [int](Get-Item $binaryPath_).Length })
    bufferViews = $bufferViews_
    accessors = $accessors_
}

$jsonPath_ = Join-Path $OutputDirectory_ "Stage_Floor05.gltf"
$json_ = $gltf_ | ConvertTo-Json -Depth 20
$json_ = $json_ -replace "`r?`n", "`r`n"
[System.IO.File]::WriteAllText($jsonPath_, $json_, [System.Text.UTF8Encoding]::new($false))
